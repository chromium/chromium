// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/one_time_passwords/otp_form_manager.h"

#include <algorithm>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/one_time_passwords/sms_otp_backend.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_constants.h"

namespace password_manager {

namespace {

using autofill::FieldGlobalId;

std::string OtpSourceToString(OtpSource source) {
  switch (source) {
    case OtpSource::kUnknown:
      return "Unknown";
    case OtpSource::kSms:
      return "Sms";
    case OtpSource::kEmail:
      return "Email";
  }
}

// TODO(crbug.com/415274388): Evaluate if FieldInfoManager needs to be improved
// to track more user inputs.
OtpSource DetermineWhereOtpWasLikelySent(FieldInfoManager* field_info_manager,
                                         const GURL& url) {
  // The manager might not exist in incognito.
  if (!field_info_manager) {
    return OtpSource::kUnknown;
  }

  std::vector<FieldInfo> field_info =
      field_info_manager->GetFieldInfo(password_manager_util::GetSignonRealm(url));

  // FieldInfoManager sorts cached fields from oldest to newest. Iterate in
  // the reverse order to check the last interacted field first.
  for (auto it = field_info.rbegin(); it != field_info.rend(); ++it) {
    if (autofill::MatchesRegex<constants::kPhoneValueRe>(it->value)) {
      return OtpSource::kSms;
    } else if (autofill::MatchesRegex<constants::kEmailValueRe>(it->value)) {
      return OtpSource::kEmail;
    }
  }
  return OtpSource::kUnknown;
}

std::vector<std::string> OtpsToSuggestionStrings(
    const std::vector<one_time_tokens::OneTimeToken>& otp_values) {
  return base::ToVector(otp_values, &one_time_tokens::OneTimeToken::value);
}

}  // namespace

OtpFormManager::OtpFormManager(
    const autofill::FormData& form_data,
    const std::vector<autofill::FieldGlobalId>& otp_field_ids,
    PasswordManagerClient* client)
    : form_data_(form_data),
      otp_field_ids_(std::move(otp_field_ids)),
      client_(client) {
  // TODO(crbug.com/415274273): Incorporate page load hints once available.
  otp_source_ = DetermineWhereOtpWasLikelySent(client_->GetFieldInfoManager(),
                                               client_->GetLastCommittedURL());

  UpdateManualTestingDebuggingDataIfNeeded();

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kAndroidSmsOtpFilling)) {
    sms_otp_backend_ = client_->GetSmsOtpBackend();
  }
#endif  // BUILDFLAG(IS_ANDROID)
  // We observe a new OTP form, we need to fetch the OTP value.
  RetrieveOtpValue();
}

OtpFormManager::~OtpFormManager() = default;

void OtpFormManager::ProcessUpdatedPredictions(
    const std::vector<autofill::FieldGlobalId>& otp_field_ids) {
  otp_field_ids_ = std::move(otp_field_ids);

  OtpSource new_otp_source = DetermineWhereOtpWasLikelySent(
      client_->GetFieldInfoManager(), client_->GetLastCommittedURL());
  if ((new_otp_source == OtpSource::kUnknown) &&
      (otp_source_ != OtpSource::kUnknown)) {
    // If the source was discovered initially, but isn't now, don't clear it -
    // it's still the best guess for now.
    return;
  }
  otp_source_ = new_otp_source;
  UpdateManualTestingDebuggingDataIfNeeded();
  // The form and the assumed OTP source have changed, we need to refetch the
  // OTP value.
  RetrieveOtpValue();
}

void OtpFormManager::ProcessServerOverrides(
    const std::vector<FieldGlobalId>& otp_overrides,
    const std::vector<FieldGlobalId>& other_overrides) {
  // Add missing OTP fields to `otp_field_ids_`.
  for (const FieldGlobalId& otp_field : otp_overrides) {
    if (std::find(otp_field_ids_.begin(), otp_field_ids_.end(), otp_field) ==
        otp_field_ids_.end()) {
      otp_field_ids_.push_back(otp_field);
    }
  }

  // Remove incorrectly classified fields from `otp_field_ids_`.
  for (const FieldGlobalId& non_otp_field : other_overrides) {
    otp_field_ids_.erase(std::remove(otp_field_ids_.begin(),
                                     otp_field_ids_.end(), non_otp_field),
                         otp_field_ids_.end());
  }
  // Run the pending callback immediately with no suggestions if no OTP
  // suggestions should be offered.
  if (otp_field_ids_.empty() && pending_suggestion_callback_) {
    std::move(pending_suggestion_callback_).Run({});
  }
}

bool OtpFormManager::IsFieldEligibleForOtpFilling(
    const FieldGlobalId& field_id) const {
  return (std::ranges::find(otp_field_ids_, field_id) !=
          otp_field_ids_.end()) &&
         (sms_otp_retrieval_in_progress_ || !otp_suggestions_.empty());
}

void OtpFormManager::GetOtpSuggestions(
    const FieldGlobalId& field_id,
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  CHECK(IsFieldEligibleForOtpFilling(field_id));
  if (!sms_otp_retrieval_in_progress_) {
    std::move(callback).Run(OtpsToSuggestionStrings(otp_suggestions_));
  } else {
    pending_suggestion_callback_ = std::move(callback);
  }
}

autofill::OtpFillData OtpFormManager::GetFillDataForOtpSuggestion(
    const FieldGlobalId& field_id,
    const std::u16string& otp_value) const {
  if (std::find(otp_field_ids_.begin(), otp_field_ids_.end(), field_id) ==
      otp_field_ids_.end()) {
    // The only way this could happen is if the form has changed between the
    // field focus and the filling moment. Fill into the current field, since
    // the user requested that and otherwise it would be weird.
    return {{field_id, otp_value}};
  }

  // If the value is longer than the number of detected fields, fill the value
  // into the triggering field.
  if (otp_value.length() > otp_field_ids_.size()) {
    return {{field_id, otp_value}};
  }

  autofill::OtpFillData fill_data;
  if (otp_value.length() == otp_field_ids_.size()) {
    // If otp_value length matches the number of fields, split the value
    // char-by-char between all fields.
    for (size_t i = 0; i < otp_field_ids_.size(); ++i) {
      fill_data[otp_field_ids_[i]] = std::u16string(1, otp_value[i]);
    }
    return fill_data;
  }

  // If OTP value length is shorter than number of fields, split it
  // char-by-char starting from the `field_id`, if it fits into the remaining
  // fields.
  size_t start_index = std::distance(
      otp_field_ids_.begin(),
      std::find(otp_field_ids_.begin(), otp_field_ids_.end(), field_id));
  if (start_index + otp_value.length() <= otp_field_ids_.size()) {
    for (size_t i = 0; i < otp_value.length(); ++i) {
      fill_data[otp_field_ids_[start_index + i]] =
          std::u16string(1, otp_value[i]);
    }
    return fill_data;
  }

  // All other cases are non-trivial, attempt to fill the value into the
  // triggering field as the best effort.
  return {{field_id, otp_value}};
}

void OtpFormManager::UpdateManualTestingDebuggingDataIfNeeded() {
  if (base::FeatureList::IsEnabled(features::kDebugUiForOtps)) {
    otp_suggestions_ = {one_time_tokens::OneTimeToken(
        // TODO(crbug.com/41527327) kSmsOtp is just a dummy value at the
        // moment. It's unclear if otp_source_ will remain in the current form.
        // Depending on that we may want to fix this or not.
        one_time_tokens::OneTimeTokenType::kSmsOtp,
        "Identified OTP field. OTP is delivered via: " +
            OtpSourceToString(otp_source_),
        base::Time::Now())};
  }
}

void OtpFormManager::RetrieveOtpValue() {
  if (sms_otp_backend_) {
    sms_otp_retrieval_in_progress_ = true;
    sms_otp_backend_->RetrieveSmsOtp(
        base::BindOnce(&OtpFormManager::OnOtpRetrievalComplete,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void OtpFormManager::OnOtpRetrievalComplete(const OtpFetchReply& reply) {
  sms_otp_retrieval_in_progress_ = false;
  if (reply.otp_value.has_value()) {
    otp_suggestions_.push_back(reply.otp_value.value());
  }

  if (pending_suggestion_callback_) {
    std::move(pending_suggestion_callback_)
        .Run(OtpsToSuggestionStrings(otp_suggestions_));
  }

  // TODO(crbug.com/415272524): Record metrics on how often the retrieval
  // succeeds or fails, in combination with the OTP source.
}

}  // namespace password_manager
