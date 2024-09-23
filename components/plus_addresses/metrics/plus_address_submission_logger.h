// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_METRICS_PLUS_ADDRESS_SUBMISSION_LOGGER_H_
#define COMPONENTS_PLUS_ADDRESSES_METRICS_PLUS_ADDRESS_SUBMISSION_LOGGER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_multi_source_observation.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/password_form_classification.h"
#include "components/autofill/core/common/unique_ids.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace plus_addresses::metrics {

class PlusAddressSubmissionLogger final : autofill::AutofillManager::Observer {
 public:
  // The prefix for all UMAs emitted by this class.
  static constexpr char kUmaSubmissionPrefix[] = "PlusAddresses.Submission";

  // A callback used to verify whether a string corresponds to a valid plus
  // address.
  using PlusAddressVerifier = base::RepeatingCallback<bool(const std::string&)>;

  PlusAddressSubmissionLogger(signin::IdentityManager* identity_manager,
                              PlusAddressVerifier plus_address_verifier);
  PlusAddressSubmissionLogger(const PlusAddressSubmissionLogger&) = delete;
  PlusAddressSubmissionLogger& operator=(const PlusAddressSubmissionLogger&) =
      delete;
  ~PlusAddressSubmissionLogger() override;

  void OnPlusAddressSuggestionShown(
      autofill::AutofillManager& manager,
      autofill::FormGlobalId form,
      autofill::FieldGlobalId field,
      autofill::AutofillPlusAddressDelegate::SuggestionContext
          suggestion_context,
      autofill::PasswordFormClassification::Type form_type,
      autofill::SuggestionType suggestion_type,
      size_t plus_address_count);

 private:
  // autofill::AutofillManager::Observer:
  void OnAutofillManagerStateChanged(
      autofill::AutofillManager& manager,
      autofill::AutofillManager::LifecycleState old_state,
      autofill::AutofillManager::LifecycleState new_state) override;
  void OnFormSubmitted(autofill::AutofillManager& manager,
                       const autofill::FormData& form) override;

  // Stops observing `manager` and removes all records for it.
  void RemoveManagerObservation(autofill::AutofillManager& manager);

  const raw_ref<signin::IdentityManager> identity_manager_;
  const PlusAddressVerifier plus_address_verifier_;

  // All records of plus address suggestions that were shown for fields. Records
  // are deleted if the form containing the field is submitted or the Autofill
  // manager that belongs to the browser form containing the form field is reset
  // or destroyed.
  struct Record final {
    Record(ukm::SourceId source_id,
           bool is_single_field_in_renderer_form,
           bool is_first_time_user);
    Record(Record&&);
    Record& operator=(Record&&);
    ~Record();

    ukm::builders::PlusAddresses_Submission ukm_builder;
    // This information for the following members is contained in the builder
    // but cannot be read from there. This duplicates it to allow recording a
    // UMA at submission.
    bool is_single_field_in_renderer_form = false;
    bool is_first_time_user = false;
  };
  // TODO: crbug.com/343124027 - Consider just keeping one map keyed by
  // FieldGlobalId once frame tokens are set on iOS.
  base::flat_map<autofill::AutofillManager*,
                 base::flat_map<autofill::FieldGlobalId, Record>>
      records_;

  // Observations of all managers for which there is an entry in `records_`. The
  // keys in `records_` and the observed managers here should always be kept
  // identical.
  base::ScopedMultiSourceObservation<autofill::AutofillManager,
                                     autofill::AutofillManager::Observer>
      managers_observation_{this};
};

}  // namespace plus_addresses::metrics

#endif  // COMPONENTS_PLUS_ADDRESSES_METRICS_PLUS_ADDRESS_SUBMISSION_LOGGER_H_
