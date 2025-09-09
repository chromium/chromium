// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_FORM_MANAGER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/integrators/password_manager/otp_delegate.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/one_time_tokens/core/browser/one_time_token.h"

namespace one_time_tokens {
struct OtpFetchReply;
class SmsOtpBackend;
}  // namespace one_time_tokens

namespace password_manager {

class PasswordManagerClient;

// Various options to where an OTP value can be sent.
enum class OtpSource {
  kUnknown = 0,
  kSms = 1,
  kEmail = 2,
};

// A class in charge of handling individual OTP forms, one instance per form.
class OtpFormManager {
 public:
  OtpFormManager(const autofill::FormData& form_data,
                 const std::vector<autofill::FieldGlobalId>& otp_field_ids,
                 PasswordManagerClient* client);

  OtpFormManager(const OtpFormManager&) = delete;
  OtpFormManager& operator=(const OtpFormManager&) = delete;

  ~OtpFormManager();

  // Forms can change dynamically during their lifetime. Ensure the most recent
  // data is used for form filling.
  void ProcessUpdatedPredictions(
      const std::vector<autofill::FieldGlobalId>& otp_field_ids);

  // Processes manual overrides coming form the server to update
  // `otp_field_ids_` if needed.
  void ProcessServerOverrides(
      const std::vector<autofill::FieldGlobalId>& otp_overrides,
      const std::vector<autofill::FieldGlobalId>& other_overrides);

  // Returns true if the field was parsed to an OTP field, and the OTP value
  // was either retrieved successfully, or the retrieval is still ongoing.
  bool IsFieldEligibleForOtpFilling(
      const autofill::FieldGlobalId& field_id) const;

  // Invokes `callback` with the OTP suggestions for a given field.
  void GetOtpSuggestions(
      const autofill::FieldGlobalId& field_id,
      base::OnceCallback<void(std::vector<std::string>)> callback);

  // Returns the fill data (mapping field IDs to values to fill) for a given
  // suggestion triggering field and OTP value.
  autofill::OtpFillData GetFillDataForOtpSuggestion(
      const autofill::FieldGlobalId& field_id,
      const std::u16string& otp_value) const;

  const std::vector<autofill::FieldGlobalId>& otp_field_ids() const {
    return otp_field_ids_;
  }

  const autofill::FormData& form_data() const { return form_data_; }

#if defined(UNIT_TEST)
  OtpSource otp_source() const { return otp_source_; }
#endif  // defined(UNIT_TEST)

 private:
  // If the kDebugUiForOtps flag is on, this method will populate
  // `otp_suggestions_` with debugging data.
  void UpdateManualTestingDebuggingDataIfNeeded();

  // Triggers the request to the appropriate backend.
  void RetrieveOtpValue();

  // Called when the OTP fetching request is complete.
  void OnOtpRetrievalComplete(const one_time_tokens::OtpFetchReply& reply);

  const autofill::FormData form_data_;

  std::vector<autofill::FieldGlobalId> otp_field_ids_;

  // The client that owns the owner of this class and is guaranteed to outlive
  // it.
  raw_ptr<PasswordManagerClient> client_;

  // Tracks where the OTP is sent to.
  OtpSource otp_source_;

  raw_ptr<one_time_tokens::SmsOtpBackend> sms_otp_backend_ = nullptr;
  bool sms_otp_retrieval_in_progress_ = false;

  // Fetched OTP values.
  std::vector<one_time_tokens::OneTimeToken> otp_suggestions_;

  // A callback stored when suggestions are queried before the OTP retrieval is
  // finished.
  base::OnceCallback<void(std::vector<std::string>)>
      pending_suggestion_callback_;

  base::WeakPtrFactory<OtpFormManager> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_FORM_MANAGER_H_
