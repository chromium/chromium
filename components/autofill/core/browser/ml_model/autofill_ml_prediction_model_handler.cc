// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ml_model/autofill_ml_prediction_model_handler.h"

#include <algorithm>
#include <vector>

#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/ml_model/autofill_model_executor.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/optimization_guide/core/model_handler.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"

namespace autofill {

namespace {

// Array describing how the output of the ML model is interpreted.
// Some of the types that the model was trained on are not supported by the
// client. Index 0 is UNKNOWN_TYPE, while the others are non-supported types.
// TODO(crbug.com/1465926): Retrieve from model metadata.
constexpr std::array<ServerFieldType, 57> kSupportedFieldTypes = {
    UNKNOWN_TYPE,
    EMAIL_ADDRESS,
    UNKNOWN_TYPE,
    UNKNOWN_TYPE,
    UNKNOWN_TYPE,
    UNKNOWN_TYPE,
    CREDIT_CARD_NUMBER,
    CONFIRMATION_PASSWORD,
    UNKNOWN_TYPE,
    PHONE_HOME_EXTENSION,
    PHONE_HOME_WHOLE_NUMBER,
    PHONE_HOME_COUNTRY_CODE,
    UNKNOWN_TYPE,
    NAME_FIRST,
    ADDRESS_HOME_DEPENDENT_LOCALITY,
    ADDRESS_HOME_CITY,
    ADDRESS_HOME_STREET_ADDRESS,
    PHONE_HOME_CITY_CODE_WITH_TRUNK_PREFIX,
    UNKNOWN_TYPE,
    NAME_HONORIFIC_PREFIX,
    CREDIT_CARD_EXP_2_DIGIT_YEAR,
    ADDRESS_HOME_STATE,
    UNKNOWN_TYPE,
    CREDIT_CARD_NAME_LAST,
    ACCOUNT_CREATION_PASSWORD,
    ADDRESS_HOME_HOUSE_NUMBER,
    PHONE_HOME_CITY_AND_NUMBER_WITHOUT_TRUNK_PREFIX,
    CREDIT_CARD_TYPE,
    CREDIT_CARD_NAME_FULL,
    ADDRESS_HOME_APT_NUM,
    CREDIT_CARD_NAME_FIRST,
    ADDRESS_HOME_FLOOR,
    UNKNOWN_TYPE,
    ADDRESS_HOME_LANDMARK,
    UNKNOWN_TYPE,
    ADDRESS_HOME_STREET_NAME,
    ADDRESS_HOME_COUNTRY,
    CREDIT_CARD_EXP_4_DIGIT_YEAR,
    DELIVERY_INSTRUCTIONS,
    PHONE_HOME_NUMBER,
    CREDIT_CARD_VERIFICATION_CODE,
    NAME_LAST,
    CREDIT_CARD_EXP_MONTH,
    ADDRESS_HOME_OVERFLOW,
    UNKNOWN_TYPE,
    NAME_FULL,
    COMPANY_NAME,
    CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR,
    PHONE_HOME_CITY_AND_NUMBER,
    PHONE_HOME_CITY_CODE,
    ADDRESS_HOME_LINE2,
    ADDRESS_HOME_STREET_LOCATION,
    ADDRESS_HOME_ZIP,
    CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
    ADDRESS_HOME_OVERFLOW_AND_LANDMARK,
    ADDRESS_HOME_LINE3,
    ADDRESS_HOME_LINE1};

}  // namespace

AutofillMlPredictionModelHandler::AutofillMlPredictionModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider)
    : optimization_guide::ModelHandler<
          AutofillModelExecutor::ModelOutput,
          const AutofillModelExecutor::ModelInput&>(
          model_provider,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
          std::make_unique<AutofillModelExecutor>(),
          /*model_inference_timeout=*/absl::nullopt,
          optimization_guide::proto::OptimizationTarget::
              OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION,
          /*model_metadata=*/absl::nullopt) {
  // Store the model in memory as soon as it is available and keep it loaded for
  // the whole browser session since we query predictions very regularly.
  // TODO(crbug.com/1465926): Maybe change both back to default behavior if we
  // see memory regressions during the rollout.
  SetShouldPreloadModel(true);
  SetShouldUnloadModelOnComplete(false);
}
AutofillMlPredictionModelHandler::~AutofillMlPredictionModelHandler() = default;

void AutofillMlPredictionModelHandler::GetModelPredictionsForForm(
    std::unique_ptr<FormStructure> form_structure,
    base::OnceCallback<void(std::unique_ptr<FormStructure>)> callback) {
  if (!ModelAvailable() || !vectorizer_) {
    // No model, no predictions.
    std::move(callback).Run(std::move(form_structure));
    return;
  }

  AutofillModelExecutor::ModelInput vectorized_input =
      VectorizeForm(*form_structure);
  ExecuteModelWithInput(
      base::BindOnce(
          [](base::WeakPtr<AutofillMlPredictionModelHandler> self,
             std::unique_ptr<FormStructure> form_structure,
             base::OnceCallback<void(std::unique_ptr<FormStructure>)> callback,
             const absl::optional<AutofillModelExecutor::ModelOutput>& output) {
            if (!self) {
              return;
            }
            CHECK(output);
            self->AssignMostLikelyTypes(*form_structure, *output);
            std::move(callback).Run(std::move(form_structure));
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(form_structure),
          std::move(callback)),
      std::move(vectorized_input));
}

void AutofillMlPredictionModelHandler::GetModelPredictionsForForms(
    std::vector<std::unique_ptr<FormStructure>> forms,
    base::OnceCallback<void(std::vector<std::unique_ptr<FormStructure>>)>
        callback) {
  auto barrier_callback = base::BarrierCallback<std::unique_ptr<FormStructure>>(
      forms.size(), std::move(callback));
  for (std::unique_ptr<FormStructure>& form : forms) {
    GetModelPredictionsForForm(std::move(form), barrier_callback);
  }
}

void AutofillMlPredictionModelHandler::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  CHECK_EQ(optimization_target,
           optimization_guide::proto::OptimizationTarget::
               OPTIMIZATION_TARGET_AUTOFILL_FIELD_CLASSIFICATION);
  optimization_guide::ModelHandler<AutofillModelExecutor::ModelOutput,
                                   const AutofillModelExecutor::ModelInput&>::
      OnModelUpdated(optimization_target, model_info);
  if (!model_info.has_value()) {
    // The model was unloaded.
    return;
  }
  // The model was loaded or updated.
  InitializeVectorizer();
}

void AutofillMlPredictionModelHandler::InitializeVectorizer() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce([] {
        return AutofillModelVectorizer::CreateVectorizer(
            base::FilePath::FromASCII(
                features::kAutofillModelDictionaryFilePath.Get()));
      }),
      base::BindOnce(
          [](base::WeakPtr<AutofillMlPredictionModelHandler> handler,
             std::unique_ptr<AutofillModelVectorizer> vectorizer) {
            if (handler) {
              CHECK(vectorizer);
              handler->vectorizer_ = std::move(vectorizer);
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
}

AutofillModelExecutor::ModelInput
AutofillMlPredictionModelHandler::VectorizeForm(
    const FormStructure& form) const {
  CHECK(vectorizer_);
  AutofillModelExecutor::ModelInput vectorized_form(form.fields().size());
  for (size_t i = 0; i < form.field_count(); ++i) {
    vectorized_form[i] = vectorizer_->Vectorize(form.field(i)->label);
  }
  return vectorized_form;
}

void AutofillMlPredictionModelHandler::AssignMostLikelyTypes(
    FormStructure& form,
    const AutofillModelExecutor::ModelOutput& output) const {
  // The model only outputs type for the first
  // `AutofillModelExecutor::kMaxNumberOfFields` many fields.
  CHECK_EQ(output.size(), std::min(form.field_count(),
                                   AutofillModelExecutor::kMaxNumberOfFields));
  for (size_t i = 0; i < output.size(); i++) {
    form.field(i)->set_heuristic_type(HeuristicSource::kMachineLearning,
                                      GetMostLikelyType(output[i]));
  }
}

ServerFieldType AutofillMlPredictionModelHandler::GetMostLikelyType(
    const std::vector<float>& model_output) const {
  size_t max_index =
      base::ranges::max_element(model_output) - model_output.begin();
  return kSupportedFieldTypes[max_index];
}

}  // namespace autofill
