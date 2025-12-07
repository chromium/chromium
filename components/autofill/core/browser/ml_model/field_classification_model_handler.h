// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_HANDLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_HANDLER_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/lru_cache.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/ml_model/field_classification_model_encoder.h"
#include "components/autofill/core/browser/ml_model/logging/autofill_ml_internals.mojom.h"
#include "components/autofill/core/browser/ml_model/logging/ml_log_router.h"
#include "components/autofill/core/browser/ml_model/model_predictions.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/inference/model_handler.h"
#include "components/optimization_guide/proto/autofill_field_classification_model_metadata.pb.h"

namespace autofill {

// Model Handler which asynchronously calls the
// `FieldClassificationModelExecutor`. It retrieves the model from the server,
// load it into memory, execute it with FormStructure as input and associate the
// model FieldType predictions with the FormStructure.
//
// Users of this class should register to asynchronous model change events via
// RegisterModelChangeCallback().
class FieldClassificationModelHandler
    : public optimization_guide::ModelHandler<
          FieldClassificationModelEncoder::ModelOutput,
          const FieldClassificationModelEncoder::ModelInput&>,
      public KeyedService {
 public:
  using ModelChangeCallbackList = base::RepeatingCallbackList<void()>;

  using ModelInputHash = size_t;

  // The version of the input, based on which the relevant model
  // version will be used by the server.
  static constexpr int64_t kAutofillModelInputVersion = 3;

  FieldClassificationModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      optimization_guide::proto::OptimizationTarget optimization_target,
      autofill::MlLogRouter* log_router = nullptr);
  ~FieldClassificationModelHandler() override;

  // This function asynchronously queries predictions for the `form_structure`
  // from the model and sets the model predictions in the FormStructure's fields
  // as heurstic type values. Once done, the `callback` is triggered on the UI
  // sequence and returns the `form_structure`. If `form_structure` has more
  // than `maximum_number_of_fields` (see model metadata) fields, it sets
  // predictions for the first `maximum_number_of_fields` fields in the form.
  //
  // NO_SERVER_DATA means the model couldn't determine the field type
  // (execution failure/low confidence). UNKNOWN_TYPE means the model is sure
  // that the field is unsupported.
  void GetModelPredictionsForForm(
      FormData form,
      const GeoIpCountryCode& client_country,
      base::OnceCallback<void(ModelPredictions)> callback);

  // Same as `GetModelPredictionsForForm()` but executes the model on multiple
  // forms.
  // Virtual for testing.
  virtual void GetModelPredictionsForForms(
      std::vector<FormData> forms,
      const GeoIpCountryCode& client_country,
      base::OnceCallback<void(std::vector<ModelPredictions>)> callback);

  // optimization_guide::ModelHandler:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  bool ShouldApplySmallFormRules() const;

  // Registers a callback that is invoked when a new model is loaded.
  [[nodiscard]] virtual base::CallbackListSubscription
  RegisterModelChangeCallback(ModelChangeCallbackList::CallbackType callback);

  optimization_guide::proto::OptimizationTarget optimization_target() const {
    return optimization_target_;
  }

#if defined(UNIT_TEST)
  const FieldTypeSet& get_supported_types() const { return supported_types_; }
#endif

 private:
  // Computes the predicted type for every element of `outputs`.
  // The size of the resulting vector is not guaranteed to have
  // `form.field_count()` elements if the maximum number of fields to be
  // predicted is limited by the model.
  std::vector<FieldType> GetMostLikelyTypes(
      const FormData& form,
      const FieldClassificationModelEncoder::ModelOutput& output) const;

  // Given the confidences returned by the ML model, returns the most likely
  // type and the confidence in it. This is currently just the argmax of
  // `model_output`, mapped to the corresponding FieldType.
  std::pair<FieldType, float> GetMostLikelyType(
      const std::vector<float>& model_output) const;

  // Applies small form rules from FormFieldParser. If triggered, sets some or
  // all values in `predicted_types` to `UNKNOWN_TYPE`. See
  // `ClearCandidatesIfHeuristicsDidNotFindEnoughFields` for details.
  // The purpose is to have identical post-processing for ML and regex
  // predictions for more accurate comparison.
  void ApplySmallFormRules(const FormData& form,
                           const GeoIpCountryCode& client_country,
                           std::vector<FieldType>& predicted_types) const;

  // Builds the predictions for the given `form`.
  ModelPredictions BuildModelPredictions(
      const FormData& form,
      base::span<const FieldType> predicted_types) const;

  // Returns true if the `output` allows to return predictions for `form`.
  bool ShouldEmitPredictions(
      const FormData& form,
      const FieldClassificationModelEncoder::ModelOutput& output);

  // Computes a hash of the encoded model input that is used as a key for
  // `predictions_cache_`.
  ModelInputHash CalculateModelInputHash(
      const FieldClassificationModelEncoder::ModelInput& input);

  autofill_ml_internals::mojom::MlPredictionLogPtr CreateMlPredictionLog(
      const FormData& form_structure) const;

  // Converts a `TokenId` to the string representation.
  std::string TokenIdToString(
      FieldClassificationModelEncoder::TokenId token_id) const;

  struct ModelState {
    optimization_guide::proto::AutofillFieldClassificationModelMetadata
        metadata;
    FieldClassificationModelEncoder encoder;
  };
  // Initialized once the model was loaded and successfully initialized using
  // the model's metadata.
  std::optional<ModelState> state_;

  // Specifies the model to load and execute.
  const optimization_guide::proto::OptimizationTarget optimization_target_;

  // Types which the model is able to output.
  FieldTypeSet supported_types_;

  // Cached model classifications.
  base::LRUCache<ModelInputHash, std::vector<FieldType>> predictions_cache_;

  ModelChangeCallbackList model_change_callback_list_;

  raw_ptr<autofill::MlLogRouter> log_router_ = nullptr;

  base::WeakPtrFactory<FieldClassificationModelHandler> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ML_MODEL_FIELD_CLASSIFICATION_MODEL_HANDLER_H_
