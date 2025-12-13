// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_METRICS_AUTOFILL_AI_LOGGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_METRICS_AUTOFILL_AI_LOGGER_H_

#include <map>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_ukm_logger.h"
#include "components/autofill/core/common/unique_ids.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace autofill {

// A class that takes care of keeping track of metric-related states and user
// interactions with forms.
class AutofillAiLogger {
 public:
  explicit AutofillAiLogger(AutofillClient* client);
  AutofillAiLogger(const AutofillAiLogger&) = delete;
  AutofillAiLogger& operator=(const AutofillAiLogger&) = delete;
  ~AutofillAiLogger();

  void OnFormHasDataToFill(FormGlobalId form_id,
                           DenseSet<EntityType> form_relevant_entity_types,
                           base::span<const EntityInstance> stored_entities);
  void OnSuggestionsShown(
      const FormStructure& form,
      const AutofillField& field,
      base::span<const EntityInstance* const> entities_suggested,
      ukm::SourceId ukm_source_id);
  void OnDidFillSuggestion(const FormStructure& form,
                           const AutofillField& field,
                           const EntityInstance& entity_filled,
                           ukm::SourceId ukm_source_id);
  void OnEditedAutofilledField(const FormStructure& form,
                               const AutofillField& field,
                               ukm::SourceId ukm_source_id);
  void OnDidFillField(const FormStructure& form,
                      const AutofillField& field,
                      const EntityInstance& entity_filled,
                      ukm::SourceId ukm_source_id);

  void OnImportPromptResult(
      const FormData& form,
      AutofillClient::AutofillAiImportPromptType prompt_type,
      EntityType entity_type,
      EntityInstance::RecordType record_type,
      AutofillClient::AutofillAiBubbleClosedReason close_reason,
      ukm::SourceId ukm_source_id);

  // Function that records the contents of `form_states` for `form` into
  // appropriate metrics. `submission_state` denotes whether the form was
  // submitted or abandoned. Also logs form-related UKM metrics.
  void RecordFormMetrics(const FormStructure& form,
                         ukm::SourceId ukm_source_id,
                         bool submission_state,
                         bool opt_in_status);

 private:
  // Helper struct that contains relevant information about the state of a form
  // regarding the AutofillAi system.
  // TODO(crbug.com/372170223): Investigate whether this can be represented as
  // an enum.
  struct FunnelState {
    // Given a form, records whether there's data available to fill this form.
    // Whether or not this data is used for filling is irrelevant.
    bool has_data_to_fill = false;
    // Given a form, records whether filling suggestions were actually shown
    // to the user.
    bool suggestions_shown = false;
    // Given a form, records whether the user chose to fill the form with a
    // filling suggestion.
    bool did_fill_suggestions = false;
    // Given a form, records whether the user corrected fields filled using
    // AutofillAi filling suggestions.
    bool edited_autofilled_field = false;
  };

  using FormFunnelStateMap =
      std::map<EntityType, std::map<EntityInstance::RecordType, FunnelState>>;

  void RecordFunnelMetrics(const FormFunnelStateMap& funnel_states,
                           bool submission_state) const;
  // Records the funnel metrics for a single `funnel_state`. `entity_type` and
  // `record_type` can be `std::nullopt`, denoting that the metrics to be
  // emitted should not be split by this information. Is is assumed that
  // `record_type` can only be non-null when `entity_type` is non-null.
  void RecordFunnelMetricsForState(
      FunnelState funnel_state,
      std::optional<EntityType> entity_type,
      std::optional<EntityInstance::RecordType> record_type,
      bool submission_state) const;
  void RecordKeyMetrics(const FormFunnelStateMap& funnel_states) const;
  // Records the key metrics for a single `funnel_state`. `entity_type` and
  // `record_type` can be `std::nullopt`, denoting that the metrics to be
  // emitted should not be split by this information. Is is assumed that
  // `record_type` can only be non-null when `entity_type` is non-null.
  void RecordKeyMetricsForState(
      FunnelState funnel_state,
      std::optional<EntityType> entity_type,
      std::optional<EntityInstance::RecordType> record_type) const;
  void RecordNumberOfFieldsFilled(const FormStructure& form,
                                  const FormFunnelStateMap& funnel_states,
                                  bool opt_in_status) const;

  // Compresses `funnel_states` into a single `FunnelState` object by
  // essentially taking the logical OR of all its boolean members.
  template <class Key>
  FunnelState CombineStates(
      const std::map<Key, FunnelState>& funnel_states) const {
    auto combine = [&](auto pred) {
      return std::ranges::any_of(funnel_states, pred,
                                 &std::pair<const Key, FunnelState>::second);
    };
    return FunnelState{
        .has_data_to_fill = combine(&FunnelState::has_data_to_fill),
        .suggestions_shown = combine(&FunnelState::suggestions_shown),
        .did_fill_suggestions = combine(&FunnelState::did_fill_suggestions),
        .edited_autofilled_field =
            combine(&FunnelState::edited_autofilled_field)};
  }

  // Records the funnel state for each form and entity type and record type
  // separately. See the documentation of `FunnelState` for more information
  // about what is recorded.
  std::map<FormGlobalId, FormFunnelStateMap> form_states_;

  // Records the IDs of forms that were submitted throughout the lifetime of
  // this object.
  std::set<FormGlobalId> submitted_forms_;

  // Records the last filled entity type and record type for each field. This
  // information is currently unavailable in `AutofillField`, because
  // `AutofillField::filling_product_` isn't accurate enough when it comes to
  // AutofillAi entities, which are all aggregated into
  // `FillingProduct::kAutofillAi`.
  // TODO(crbug.com/432650464): Update this state on Undo operations.
  std::map<FieldGlobalId, std::pair<EntityType, EntityInstance::RecordType>>
      last_filled_entity_;

  AutofillAiUkmLogger ukm_logger_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_AUTOFILL_AI_METRICS_AUTOFILL_AI_LOGGER_H_
