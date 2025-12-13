// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

#include "base/i18n/message_formatter.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/device_info.h"
#endif

namespace password_manager {

using metrics_util::LeakDialogType;

constexpr char kPasswordCheckupURL[] =
    "https://passwords.google.com/checkup/start?hideExplanation=true";

LeakedPasswordDetails::LeakedPasswordDetails(CredentialLeakType leak_type,
                                             PasswordForm credentials,
                                             bool in_account_store)
    : leak_type(leak_type),
      credentials(std::move(credentials)),
      in_account_store(in_account_store) {}
LeakedPasswordDetails::LeakedPasswordDetails(const LeakedPasswordDetails&) =
    default;
LeakedPasswordDetails::LeakedPasswordDetails(LeakedPasswordDetails&& other) =
    default;
LeakedPasswordDetails::~LeakedPasswordDetails() = default;

LeakedPasswordDetails& LeakedPasswordDetails::operator=(
    const LeakedPasswordDetails&) = default;
LeakedPasswordDetails& LeakedPasswordDetails::operator=(
    LeakedPasswordDetails&& other) = default;

CredentialLeakType CreateLeakType(IsSaved is_saved,
                                  IsReused is_reused,
                                  IsSyncing is_syncing,
                                  HasChangePasswordUrl has_change_password,
                                  IsSavedAsBackup is_saved_as_backup) {
  CredentialLeakType leak_type = 0;
  if (is_saved) {
    leak_type |= kPasswordSaved;
  }
  if (is_reused) {
    leak_type |= kPasswordUsedOnOtherSites;
  }
  if (is_syncing) {
    leak_type |= kPasswordSynced;
  }
  if (has_change_password) {
    leak_type |= kHasChangePasswordUrl;
  }
  if (is_saved_as_backup) {
    leak_type |= kPasswordSavedAsBackup;
  }
  return leak_type;
}

bool IsPasswordSaved(CredentialLeakType leak_type) {
  return leak_type & CredentialLeakFlags::kPasswordSaved;
}

bool IsPasswordSavedAsBackup(CredentialLeakType leak_type) {
  return leak_type & CredentialLeakFlags::kPasswordSavedAsBackup;
}

bool IsPasswordUsedOnOtherSites(CredentialLeakType leak_type) {
  return leak_type & CredentialLeakFlags::kPasswordUsedOnOtherSites;
}

bool IsPasswordSynced(CredentialLeakType leak_type) {
  return leak_type & CredentialLeakFlags::kPasswordSynced;
}

bool IsPasswordChangeSupported(CredentialLeakType leak_type) {
  return leak_type & CredentialLeakFlags::kHasChangePasswordUrl;
}

// Formats the `origin` to a human-friendly url string.
std::u16string GetFormattedUrl(const GURL& origin) {
  return url_formatter::FormatUrlForSecurityDisplay(
      origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
}

std::u16string GetLeakDetectionTooltip() {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_LEAK_HELP_MESSAGE);
}

bool ShouldCheckPasswords(CredentialLeakType leak_type) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::device_info::is_automotive()) {
    return false;
  }
#endif
  return password_manager::IsPasswordUsedOnOtherSites(leak_type);
}

LeakDialogType GetLeakDialogType(CredentialLeakType leak_type) {
  if (!ShouldCheckPasswords(leak_type)) {
    return LeakDialogType::kChange;
  }

  return password_manager::IsPasswordSaved(leak_type)
             ? LeakDialogType::kCheckup
             : LeakDialogType::kCheckupAndChange;
}

GURL GetPasswordCheckupURL(PasswordCheckupReferrer referrer) {
  GURL url(kPasswordCheckupURL);
  url = net::AppendQueryParameter(url, "utm_source", "chrome");

#if BUILDFLAG(IS_ANDROID)
  const char* const medium = "android";
#elif BUILDFLAG(IS_IOS)
  const char* const medium = "ios";
#else
  const char* const medium = "desktop";
#endif
  url = net::AppendQueryParameter(url, "utm_medium", medium);

  const char* const campaign =
      referrer == PasswordCheckupReferrer::kLeakDetectionDialog
          ? "leak_dialog"
          : "password_settings";

  return net::AppendQueryParameter(url, "utm_campaign", campaign);
}

std::unique_ptr<LeakDialogTraits> CreateDialogTraits(
    CredentialLeakType leak_type) {
  switch (password_manager::GetLeakDialogType(leak_type)) {
    case LeakDialogType::kChange:
      return std::make_unique<LeakDialogTraitsImp<LeakDialogType::kChange>>();
    case LeakDialogType::kCheckup:
      return std::make_unique<LeakDialogTraitsImp<LeakDialogType::kCheckup>>();
    case LeakDialogType::kCheckupAndChange:
      return std::make_unique<
          LeakDialogTraitsImp<LeakDialogType::kCheckupAndChange>>();
  }
}

}  // namespace password_manager
