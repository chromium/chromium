// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_FORM_MANAGER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/unique_ids.h"

namespace password_manager {

class PasswordManagerClient;
class SmsOtpBackend;

// Various options to where an OTP value can be sent.
enum class OtpSource {
  kUnknown = 0,
  kSms = 1,
  kEmail = 2,
};

struct OtpFetchReply;

// A class in charge of handling individual OTP forms, one instance per form.
class OtpFormManager {
 public:
  OtpFormManager(autofill::FormGlobalId form_id,
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

  const std::vector<autofill::FieldGlobalId>& otp_field_ids() const {
    return otp_field_ids_;
  }

#if defined(UNIT_TEST)
  OtpSource otp_source() const { return otp_source_; }
#endif  // defined(UNIT_TEST)

 private:
  // Triggers the request to the appropriate backend.
  void RetrieveOtpValue();

  // Called when the OTP fetching request is complete.
  void OnOtpRetrievalComplete(const OtpFetchReply& reply);

  autofill::FormGlobalId form_id_;

  std::vector<autofill::FieldGlobalId> otp_field_ids_;

  // The client that owns the owner of this class and is guaranteed to outlive
  // it.
  raw_ptr<PasswordManagerClient> client_;

  // Tracks where the OTP is sent to.
  OtpSource otp_source_;

  raw_ptr<SmsOtpBackend> sms_otp_backend_ = nullptr;
  bool sms_otp_retrieval_in_progress_ = false;

  // Fetched OTP values.
  std::vector<std::string> otp_suggestions_;

  // A callback stored when suggestions are queried before the OTP retrieval is
  // finished.
  base::OnceCallback<void(std::vector<std::string>)>
      pending_suggestion_callback_;

  base::WeakPtrFactory<OtpFormManager> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ONE_TIME_PASSWORDS_OTP_FORM_MANAGER_H_
