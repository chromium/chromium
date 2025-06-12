// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"

#include <algorithm>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_encoder.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_executor.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/inference/model_handler.h"
#include "components/optimization_guide/proto/autofill_field_classification_model_metadata.pb.h"

namespace autofill {

namespace {

// Needed to not allow the predictions cache to grow unlimited during long
// Desktop sessions.
constexpr size_t kMaxPredictionsToCache = 100;

// Creates the model metadata and specifies the model input version to
// ensure client-server version compatibility while loading the model.
std::optional<optimization_guide::proto::Any> CreateModelMetadata() {
  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/"
      "google.internal.chrome.optimizationguide.v1."
      "AutofillFieldClassificationModelMetadata");
  optimization_guide::proto::AutofillFieldClassificationModelMetadata
      model_metadata;
  model_metadata.set_input_version(
      FieldClassificationModelHandler::kAutofillModelInputVersion);
  model_metadata.SerializeToString(any_metadata.mutable_value());
  return any_metadata;
}

// Returns true if all fields can be predicted at confidence >=
// `confidence_threshold`.
bool AllFieldsClassifiedWithConfidence(
    const FieldClassificationModelEncoder::ModelOutput& output,
    size_t num_fields,
    float confidence_threshold) {
  for (size_t i = 0; i < num_fields; i++) {
    if (std::ranges::max(output[i]) < confidence_threshold) {
      return false;
    }
  }
  return true;
}

HeuristicSource GetHeuristicSource(
    optimization_guide::proto::OptimizationTarget optimization_target) {
  switch (optimization_target) {
    case optimization_guide::proto::OptimizationTarget::
        OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION:
      return HeuristicSource::kAutofillMachineLearning;
    case optimization_guide::proto::OptimizationTarget::
        OPTIMIZATION_TARGET_PASSWORD_MANAGER_FORM_CLASSIFICATION:
      return HeuristicSource::kPasswordManagerMachineLearning;
    default:
      NOTREACHED();
  }
}

bool ParsingSupportsMultipleFieldsOfType(FieldType type) {
  switch (type) {
    case USERNAME:
    case ACCOUNT_CREATION_PASSWORD:
    case CONFIRMATION_PASSWORD:
    case PASSWORD:
      return false;
    default:
      return true;
  }
}

}  // anonymous namespace

FieldClassificationModelHandler::FieldClassificationModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    optimization_guide::proto::OptimizationTarget optimization_target)
    : optimization_guide::ModelHandler<
          FieldClassificationModelEncoder::ModelOutput,
          const FieldClassificationModelEncoder::ModelInput&>(
          model_provider,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
          std::make_unique<FieldClassificationModelExecutor>(),
          /*model_inference_timeout=*/std::nullopt,
          optimization_target,
          CreateModelMetadata()),
      optimization_target_(optimization_target),
      predictions_cache_(kMaxPredictionsToCache) {
  // Store the model in memory as soon as it is available and keep it loaded for
  // the whole browser session since we query predictions very regularly.
  // TODO(crbug.com/40276177): Maybe change both back to default behavior if we
  // see memory regressions during the rollout.
  SetShouldPreloadModel(true);
  SetShouldUnloadModelOnComplete(false);
}
FieldClassificationModelHandler::~FieldClassificationModelHandler() = default;

bool FieldClassificationModelHandler::ShouldApplySmallFormRules() const {
  return (optimization_target_ ==
          optimization_guide::proto::OptimizationTarget::
              OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION) &&
         features::kAutofillModelPredictionsSmallFormRules.Get();
}

void FieldClassificationModelHandler::ApplySmallFormRules(
    const FormStructure& form,
    std::vector<FieldType>& predicted_types) const {
  FieldCandidatesMap field_candidates_map;
  for (size_t i = 0; i < predicted_types.size(); ++i) {
    FieldCandidates candidates;
    candidates.AddFieldCandidate(
        predicted_types[i],
        // Arbitrary value to satisfy the API - not used.
        MatchAttribute::kLabel, 1.0f);
    field_candidates_map.try_emplace(form.field(i)->global_id(),
                                     std::move(candidates));
  }

  FormFieldParser::ClearCandidatesIfHeuristicsDidNotFindEnoughFields(
      form.fields(), field_candidates_map, form.is_form_element(),
      form.client_country(), nullptr);

  for (size_t i = 0; i < predicted_types.size(); ++i) {
    const auto& field_id = form.field(i)->global_id();
    if (!field_candidates_map.contains(field_id)) {
      if (predicted_types[i] == NO_SERVER_DATA) {
        // Leave NO_SERVER_DATA predictions as is, give a chance to regex to
        // overwrite them.
        continue;
      }
      // The field was cleared by the small form rules.
      predicted_types[i] = UNKNOWN_TYPE;
    }
  }
}

void FieldClassificationModelHandler::GetModelPredictionsForForm(
    std::unique_ptr<FormStructure> form_structure,
    base::OnceCallback<void(std::unique_ptr<FormStructure>)> callback) {
  if (!ModelAvailable() || !state_) {
    // No model, no predictions.
    std::move(callback).Run(std::move(form_structure));
    return;
  }

  FieldClassificationModelEncoder::ModelInput encoded_input =
      state_->encoder.EncodeForm(*form_structure);

  std::optional<ModelInputHash> input_hash;
  if (base::FeatureList::IsEnabled(
          features::kFieldClassificationModelCaching)) {
    // Check if the model has already ran for the same inputs.
    input_hash = CalculateModelInputHash(encoded_input);
    auto cached_result = predictions_cache_.Get(input_hash.value());
    if (cached_result != predictions_cache_.end()) {
      // Do not use cached results if the number of classified fields does not
      // correspond the number of fields in the observed form % the max number
      // of fields that the model is able to classify.
      if (cached_result->second.size() ==
          std::min(form_structure->field_count(),
                   static_cast<size_t>(state_->metadata.encoding_parameters()
                                           .maximum_number_of_fields()))) {
        AssignPredictedFieldTypesToForm(cached_result->second, *form_structure);
        std::move(callback).Run(std::move(form_structure));
        return;
      }
    }
  }

  ExecuteModelWithInput(
      base::BindOnce(
          [](base::WeakPtr<FieldClassificationModelHandler> self,
             std::unique_ptr<FormStructure> form_structure,
             std::optional<ModelInputHash> model_input_hash,
             base::OnceCallback<void(std::unique_ptr<FormStructure>)> callback,
             const std::optional<FieldClassificationModelEncoder::ModelOutput>&
                 output) {
            if (self && output &&
                self->ShouldEmitPredictions(form_structure.get(), *output)) {
              std::vector<FieldType> predicted_types =
                  self->GetMostLikelyTypes(*form_structure, *output);
              if (self->ShouldApplySmallFormRules()) {
                self->ApplySmallFormRules(*form_structure, predicted_types);
              }
              self->AssignPredictedFieldTypesToForm(predicted_types,
                                                    *form_structure);
              if (model_input_hash.has_value()) {
                self->predictions_cache_.Put(model_input_hash.value(),
                                             predicted_types);
              }
            }
            std::move(callback).Run(std::move(form_structure));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(form_structure),
          std::move(input_hash), std::move(callback)),
      std::move(encoded_input));
}

void FieldClassificationModelHandler::GetModelPredictionsForForms(
    std::vector<std::unique_ptr<FormStructure>> forms,
    base::OnceCallback<void(std::vector<std::unique_ptr<FormStructure>>)>
        callback) {
  auto barrier_callback = base::BarrierCallback<std::unique_ptr<FormStructure>>(
      forms.size(), std::move(callback));
  for (std::unique_ptr<FormStructure>& form : forms) {
    GetModelPredictionsForForm(std::move(form), barrier_callback);
  }
}

void FieldClassificationModelHandler::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  CHECK_EQ(optimization_target, optimization_target_);
  optimization_guide::ModelHandler<
      FieldClassificationModelEncoder::ModelOutput,
      const FieldClassificationModelEncoder::ModelInput&>::
      OnModelUpdated(optimization_target, model_info);
  if (!model_info.has_value()) {
    // The model was unloaded.
    return;
  }
  // The model was loaded or updated.
  state_.reset();
  ModelState state;
  if (!model_info->GetModelMetadata() ||
      !state.metadata.ParseFromString(
          model_info->GetModelMetadata()->value())) {
    // The model should always come with metadata - but since this comes from
    // the server-side and might change in the future, it might fail.
    return;
  }
  state.encoder = FieldClassificationModelEncoder(
      state.metadata.input_token(), state.metadata.encoding_parameters());
  supported_types_.clear();
  for (int type : state.metadata.output_type()) {
    supported_types_.insert(ToSafeFieldType(FieldType(type), NO_SERVER_DATA));
  }
  state_.emplace(std::move(state));

  // Invalidate cached predictions, if any.
  predictions_cache_.Clear();
}

std::vector<FieldType> FieldClassificationModelHandler::GetMostLikelyTypes(
    FormStructure& form,
    const FieldClassificationModelEncoder::ModelOutput& output) const {
  // The ML model can process at most
  // `FieldClassificationModelEncoder::kModelMaxNumberOfFields`.
  size_t relevant_fields = std::min(form.field_count(), output.size());

  // Some field types and model metadata do not allow assigning the same type to
  // multiple fields. If the type requires to pick a single field, track which
  // field was assigned to the type, and with which confidence.
  std::map<FieldType, std::pair<size_t, float>> unique_types_assignment;
  std::vector<FieldType> predicted_types;

  for (size_t i = 0; i < relevant_fields; i++) {
    auto [most_likely_type, current_confidence] = GetMostLikelyType(output[i]);

    if (state_->metadata.postprocessing_parameters()
            .disallow_same_type_predictions() &&
        !ParsingSupportsMultipleFieldsOfType(most_likely_type) &&
        unique_types_assignment.contains(most_likely_type)) {
      auto [previous_field_index, previous_field_confidence] =
          unique_types_assignment[most_likely_type];
      if (current_confidence > previous_field_confidence) {
        // Remove the type assignment for the previously selected field index.
        predicted_types[previous_field_index] = NO_SERVER_DATA;
      } else {
        most_likely_type = NO_SERVER_DATA;
      }
    }

    if (!ParsingSupportsMultipleFieldsOfType(most_likely_type)) {
      unique_types_assignment[most_likely_type] = {i, current_confidence};
    }
    predicted_types.push_back(most_likely_type);
  }
  return predicted_types;
}

std::pair<FieldType, float> FieldClassificationModelHandler::GetMostLikelyType(
    const std::vector<float>& model_output) const {
  CHECK(state_);
  int max_index = std::ranges::max_element(model_output) - model_output.begin();
  CHECK_LT(max_index, state_->metadata.output_type_size());
  if (!state_->metadata.postprocessing_parameters()
           .has_confidence_threshold_per_field() ||
      model_output[max_index] >= state_->metadata.postprocessing_parameters()
                                     .confidence_threshold_per_field()) {
    return {
        ToSafeFieldType(state_->metadata.output_type(max_index), UNKNOWN_TYPE),
        model_output[max_index]};
  }
  return {NO_SERVER_DATA, 0.0};
}

void FieldClassificationModelHandler::AssignPredictedFieldTypesToForm(
    const std::vector<FieldType>& predicted_types,
    FormStructure& form) {
  size_t num_predicted_fields =
      std::min(form.field_count(), predicted_types.size());
  HeuristicSource heuristic_source = GetHeuristicSource(optimization_target_);

  for (size_t i = 0; i < num_predicted_fields; i++) {
    form.field(i)->set_ml_supported_types(supported_types_);
    form.field(i)->set_heuristic_type(heuristic_source, predicted_types[i]);
  }
}

bool FieldClassificationModelHandler::ShouldEmitPredictions(
    const FormStructure* form,
    const FieldClassificationModelEncoder::ModelOutput& output) {
  return !state_->metadata.postprocessing_parameters()
              .has_confidence_threshold_to_disable_all_predictions() ||
         AllFieldsClassifiedWithConfidence(
             output, std::min(form->field_count(), output.size()),
             state_->metadata.postprocessing_parameters()
                 .confidence_threshold_to_disable_all_predictions());
}

FieldClassificationModelHandler::ModelInputHash
FieldClassificationModelHandler::CalculateModelInputHash(
    const FieldClassificationModelEncoder::ModelInput& input) {
  // Flatten the data for hashing.
  size_t flattened_data_size = 0;
  for (const std::vector<FieldClassificationModelEncoder::TokenId>&
           field_tokens : input) {
    flattened_data_size += field_tokens.size();
  }
  std::vector<FieldClassificationModelEncoder::TokenId> flattened_data;
  flattened_data.reserve(flattened_data_size);
  for (const std::vector<FieldClassificationModelEncoder::TokenId>&
           field_tokens : input) {
    flattened_data.insert(flattened_data.end(), field_tokens.begin(),
                          field_tokens.end());
  }

  return ModelInputHash(base::FastHash(base::as_byte_span(flattened_data)));
}

}  // namespace autofill
