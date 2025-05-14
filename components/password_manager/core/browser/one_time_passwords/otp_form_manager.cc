// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/one_time_passwords/otp_form_manager.h"

#include "base/feature_list.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/one_time_passwords/sms_otp_backend.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_constants.h"

namespace password_manager {

namespace {

// TODO(crbug.com/415274388): Evaluate if FieldInfoManager needs to be improved
// to track more user inputs.
OtpSource DetermineWhereOtpWasLikelySent(FieldInfoManager* field_info_manager,
                                         const GURL& url) {
  // The manager might not exist in incognito.
  if (!field_info_manager) {
    return OtpSource::kUnknown;
  }

  std::vector<FieldInfo> field_info =
      field_info_manager->GetFieldInfo(GetSignonRealm(url));

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

}  // namespace

OtpFormManager::OtpFormManager(
    autofill::FormGlobalId form_id,
    const std::vector<autofill::FieldGlobalId>& otp_field_ids,
    PasswordManagerClient* client)
    : form_id_(form_id),
      otp_field_ids_(std::move(otp_field_ids)),
      client_(client) {
  // TODO(crbug.com/415274273): Incorporate page load hints once available.
  otp_source_ = DetermineWhereOtpWasLikelySent(client_->GetFieldInfoManager(),
                                               client_->GetLastCommittedURL());

#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kAndroidSmsOtpFilling)) {
    sms_otp_backend_ = client_->GetSmsOtpBackend();
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

OtpFormManager::OtpFormManager(OtpFormManager&&) = default;
OtpFormManager& OtpFormManager::operator=(OtpFormManager&&) = default;

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
}

}  // namespace password_manager
