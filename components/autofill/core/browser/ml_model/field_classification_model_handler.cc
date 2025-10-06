// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/field_classification_model_handler.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/types/zip.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_encoder.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_executor.h"
#include "components/autofill/core/browser/ml_model/logging/autofill_ml_internals.mojom.h"
#include "components/autofill/core/browser/ml_model/model_predictions.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
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
    optimization_guide::proto::OptimizationTarget optimization_target,
    MlLogRouter* log_router)
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
      predictions_cache_(kMaxPredictionsToCache),
      log_router_(log_router) {
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
    const FormData& form,
    const GeoIpCountryCode& client_country,
    std::vector<FieldType>& predicted_types) const {
  FieldCandidatesMap field_candidates_map;
  for (size_t i = 0; i < predicted_types.size(); ++i) {
    FieldCandidates candidates;
    candidates.AddFieldCandidate(
        predicted_types[i],
        // Arbitrary value to satisfy the API - not used.
        MatchAttribute::kLabel, 1.0f);
    field_candidates_map.try_emplace(form.fields()[i].global_id(),
                                     std::move(candidates));
  }

  FormFieldParser::ClearCandidatesIfHeuristicsDidNotFindEnoughFields(
      form.fields(), field_candidates_map, client_country, nullptr);

  for (size_t i = 0; i < predicted_types.size(); ++i) {
    const auto& field_id = form.fields()[i].global_id();
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

autofill_ml_internals::mojom::MlPredictionLogPtr
FieldClassificationModelHandler::CreateMlPredictionLog(
    const FormData& form) const {
  autofill_ml_internals::mojom::MlPredictionLogPtr prediction_log =
      autofill_ml_internals::mojom::MlPredictionLog::New();

  switch (optimization_target_) {
    case optimization_guide::proto::OptimizationTarget::
        OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION:
      prediction_log->optimization_target =
          autofill_ml_internals::mojom::OptimizationTarget::kAutofill;
      break;
    case optimization_guide::proto::OptimizationTarget::
        OPTIMIZATION_TARGET_PASSWORD_MANAGER_FORM_CLASSIFICATION:
      prediction_log->optimization_target =
          autofill_ml_internals::mojom::OptimizationTarget::kPassword;
      break;
    default:
      prediction_log->optimization_target =
          autofill_ml_internals::mojom::OptimizationTarget::kUnknown;
      break;
  }

  prediction_log->form_signature =
      base::NumberToString(*CalculateFormSignature(form));
  prediction_log->form_url = form.url();

  std::vector<std::string> model_types;
  for (int field_type_as_int : state_->metadata.output_type()) {
    FieldType field_type =
        ToSafeFieldType(FieldType(field_type_as_int), NO_SERVER_DATA);
    std::string field_type_name = (field_type != NO_SERVER_DATA)
                                      ? FieldTypeToString(field_type)
                                      : "[INVALID]";
    model_types.emplace_back(std::move(field_type_name));
  }
  prediction_log->model_output_types.assign(model_types.begin(),
                                            model_types.end());

  std::vector<autofill_ml_internals::mojom::MlFieldPredictionLogPtr>
      field_predictions;
  for (const auto& field : form.fields()) {
    autofill_ml_internals::mojom::MlFieldPredictionLogPtr field_prediction =
        autofill_ml_internals::mojom::MlFieldPredictionLog::New();
    field_prediction->label = base::UTF16ToUTF8(field.label());
    field_prediction->placeholder = base::UTF16ToUTF8(field.placeholder());
    field_prediction->autocomplete = field.autocomplete_attribute();
    field_prediction->name = base::UTF16ToUTF8(field.name_attribute());
    field_prediction->id = base::UTF16ToUTF8(field.id_attribute());
    field_prediction->form_control_type =
        FormControlTypeToString(field.form_control_type());

    std::vector<autofill_ml_internals::mojom::SelectOptionPtr> select_options;
    for (const auto& option : field.options()) {
      autofill_ml_internals::mojom::SelectOptionPtr logged_option =
          autofill_ml_internals::mojom::SelectOption::New();
      logged_option->value = base::UTF16ToUTF8(option.value);
      logged_option->text = base::UTF16ToUTF8(option.text);
    }
    field_prediction->select_options.assign(
        std::make_move_iterator(select_options.begin()),
        std::make_move_iterator(select_options.end()));

    field_predictions.emplace_back(std::move(field_prediction));
  }
  prediction_log->field_predictions.assign(
      std::make_move_iterator(field_predictions.begin()),
      std::make_move_iterator(field_predictions.end()));

  prediction_log->start_time = base::Time::Now();
  return prediction_log;
}

void PopulateMlPredictionLogAfterInference(
    autofill_ml_internals::mojom::MlPredictionLog& prediction_log,
    const FieldClassificationModelEncoder::ModelOutput& model_output) {
  prediction_log.end_time = base::Time::Now();
  prediction_log.duration = prediction_log.end_time - prediction_log.start_time;

  // Cannot zip this: `model_output` has a fixed length (e.g. 30), which can be
  // both shorter and longer than the number of fields.
  for (size_t i = 0;
       i < model_output.size() && i < prediction_log.field_predictions.size();
       ++i) {
    prediction_log.field_predictions[i]->probabilities = model_output[i];
  }
}

void FieldClassificationModelHandler::GetModelPredictionsForForm(
    FormData form,
    const GeoIpCountryCode& client_country,
    base::OnceCallback<void(ModelPredictions)> callback) {
  if (!ModelAvailable() || !state_) {
    // No model, no predictions.
    std::move(callback).Run(BuildModelPredictions(form, {}));
    return;
  }

  std::optional<autofill_ml_internals::mojom::MlPredictionLogPtr>
      prediction_log = std::nullopt;
  if (log_router_ && log_router_->HasReceivers()) {
    prediction_log = CreateMlPredictionLog(form);
  }

  FieldClassificationModelEncoder::ModelInput encoded_input =
      state_->encoder.EncodeForm(form);

  if (prediction_log) {
    for (auto [encoded_field, field_prediction] :
         base::zip(encoded_input, prediction_log.value()->field_predictions)) {
      field_prediction->tokenized_field_representation = base::ToVector(
          encoded_field,
          [this](FieldClassificationModelEncoder::TokenId token_id) {
            return TokenIdToString(token_id);
          });
    }
  }

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
          std::min(form.fields().size(),
                   static_cast<size_t>(state_->metadata.encoding_parameters()
                                           .maximum_number_of_fields()))) {
        std::move(callback).Run(
            BuildModelPredictions(form, cached_result->second));
        return;
      }
    }
  }

  ExecuteModelWithInput(
      base::BindOnce(
          [](base::WeakPtr<FieldClassificationModelHandler> self,
             std::optional<autofill_ml_internals::mojom::MlPredictionLogPtr>
                 prediction_log,
             FormData form, const GeoIpCountryCode& client_country,
             std::optional<ModelInputHash> model_input_hash,
             base::OnceCallback<void(ModelPredictions)> callback,
             const std::optional<FieldClassificationModelEncoder::ModelOutput>&
                 output) {
            if (!self) {
              return;
            }
            ModelPredictions predictions =
                self->BuildModelPredictions(form, {});
            if (output && self->ShouldEmitPredictions(form, *output)) {
              std::vector<FieldType> predicted_types =
                  self->GetMostLikelyTypes(form, *output);
              if (self->ShouldApplySmallFormRules()) {
                self->ApplySmallFormRules(form, client_country,
                                          predicted_types);
              }
              predictions = self->BuildModelPredictions(form, predicted_types);
              if (model_input_hash.has_value()) {
                self->predictions_cache_.Put(model_input_hash.value(),
                                             predicted_types);
              }
            }
            if (output && prediction_log && self->log_router_) {
              PopulateMlPredictionLogAfterInference(*prediction_log.value(),
                                                    output.value());
              self->log_router_->ProcessLog(std::move(*prediction_log));
            }
            std::move(callback).Run(std::move(predictions));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(prediction_log),
          std::move(form), client_country, std::move(input_hash),
          std::move(callback)),
      std::move(encoded_input));
}

void FieldClassificationModelHandler::GetModelPredictionsForForms(
    std::vector<FormData> forms,
    const GeoIpCountryCode& client_country,
    base::OnceCallback<void(std::vector<ModelPredictions>)> callback) {
  // `base::BarrierCallback` is not guaranteed to gather the results in order so
  // we have to sort them.
  using IndexAndOutput = std::pair<size_t, ModelPredictions>;
  auto sort_results =
      base::BindOnce([](std::vector<IndexAndOutput> indexed_predictions)
                         -> std::vector<ModelPredictions> {
        std::ranges::sort(indexed_predictions,
                          [](IndexAndOutput& a, IndexAndOutput& b) {
                            return a.first < b.first;
                          });
        return base::ToVector(std::move(indexed_predictions),
                              [](IndexAndOutput& p) -> ModelPredictions&& {
                                return std::move(p.second);
                              });
      });
  auto barrier_callback = base::BarrierCallback<IndexAndOutput>(
      forms.size(), std::move(sort_results).Then(std::move(callback)));
  for (size_t form_index = 0; form_index < forms.size(); ++form_index) {
    GetModelPredictionsForForm(
        std::move(forms[form_index]), client_country,
        base::BindOnce(
            [](size_t form_index,
               ModelPredictions prediction) -> IndexAndOutput {
              return {form_index, std::move(prediction)};
            },
            form_index)
            .Then(barrier_callback));
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

  model_change_callback_list_.Notify();
}

base::CallbackListSubscription
FieldClassificationModelHandler::RegisterModelChangeCallback(
    ModelChangeCallbackList::CallbackType callback) {
  return model_change_callback_list_.Add(std::move(callback));
}

std::vector<FieldType> FieldClassificationModelHandler::GetMostLikelyTypes(
    const FormData& form,
    const FieldClassificationModelEncoder::ModelOutput& output) const {
  // The ML model can process at most
  // `FieldClassificationModelEncoder::kModelMaxNumberOfFields`.
  size_t relevant_fields = std::min(form.fields().size(), output.size());

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

ModelPredictions FieldClassificationModelHandler::BuildModelPredictions(
    const FormData& form,
    base::span<const FieldType> predicted_types) const {
  std::vector<std::pair<FieldGlobalId, FieldType>> field_predictions;
  field_predictions.reserve(predicted_types.size());
  for (auto [field, type] : base::zip(form.fields(), predicted_types)) {
    field_predictions.emplace_back(field.global_id(), type);
  }
  return ModelPredictions(GetHeuristicSource(optimization_target_),
                          supported_types_, std::move(field_predictions));
}

bool FieldClassificationModelHandler::ShouldEmitPredictions(
    const FormData& form,
    const FieldClassificationModelEncoder::ModelOutput& output) {
  return !state_->metadata.postprocessing_parameters()
              .has_confidence_threshold_to_disable_all_predictions() ||
         AllFieldsClassifiedWithConfidence(
             output, std::min(form.fields().size(), output.size()),
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

std::string FieldClassificationModelHandler::TokenIdToString(
    FieldClassificationModelEncoder::TokenId token_id) const {
  const int token = static_cast<int>(token_id.value());
  if (token == 0) {
    // Padding token, always encoded as 0.
    return "";
  } else if (token == 1) {
    // Unknown, out-of-vocabulary token, always encoded as 1.
    return "[UNK]";
  } else if (token == state_->metadata.input_token_size() + 2) {
    // Special "classification" token used by the model, encoded as
    // `vocabulary_size` (where the vocab size includes the two special tokens,
    // hence the +2).
    return "[CLS]";
  } else if (token < 2 || token >= state_->metadata.input_token_size() + 2) {
    return "[INVALID]";
  } else {
    // Indexing starts at 2 because of the special tokens.
    return state_->metadata.input_token(token_id.value() - 2);
  }
}

}  // namespace autofill
