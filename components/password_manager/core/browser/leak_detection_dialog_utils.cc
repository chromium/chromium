// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

#include "base/i18n/message_formatter.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace password_manager {

using metrics_util::LeakDialogType;

constexpr char kPasswordCheckupURL[] =
    "https://passwords.google.com/checkup/start?hideExplanation=true";

CredentialLeakType CreateLeakType(IsSaved is_saved,
                                  IsReused is_reused,
                                  IsSyncing is_syncing) {
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
  return leak_type;
}

bool IsPasswordSaved(CredentialLeakType leak_type) {
  return leak_type & CredentialLeakFlags::kPasswordSaved;
}

bool IsPasswordUsedOnOtherSites(CredentialLeakType leak_type) {
  return leak_type & CredentialLeakFlags::kPasswordUsedOnOtherSites;
}

bool IsPasswordSynced(CredentialLeakType leak_type) {
  return leak_type & CredentialLeakFlags::kPasswordSynced;
}

// Formats the `origin` to a human-friendly url string.
std::u16string GetFormattedUrl(const GURL& origin) {
  return url_formatter::FormatUrlForSecurityDisplay(
      origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
}

std::u16string GetAcceptButtonLabel(CredentialLeakType leak_type) {
  if (ShouldCheckPasswords(leak_type)) {
    return l10n_util::GetStringUTF16(IDS_LEAK_CHECK_CREDENTIALS);
  }

  return l10n_util::GetStringUTF16(IDS_OK);
}

std::u16string GetCancelButtonLabel(CredentialLeakType leak_type) {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

std::u16string GetDescription(CredentialLeakType leak_type) {
  if (!ShouldCheckPasswords(leak_type)) {
    return l10n_util::GetStringUTF16(
        UsesPasswordManagerGoogleBranding()
            ? IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_BRANDED
            : IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE_GPM_NON_BRANDED);
  }
  if (password_manager::IsPasswordSaved(leak_type)) {
    return l10n_util::GetStringUTF16(
        UsesPasswordManagerGoogleBranding()
            ? IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE_GPM_BRANDED
            : IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE_GPM_NON_BRANDED);
  }
  return l10n_util::GetStringUTF16(
      UsesPasswordManagerGoogleBranding()
          ? IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE_GPM_BRANDED
          : IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE_GPM_NON_BRANDED);
}

std::u16string GetTitle(CredentialLeakType leak_type) {
  return l10n_util::GetStringUTF16(ShouldCheckPasswords(leak_type)
                                       ? IDS_CREDENTIAL_LEAK_TITLE_CHECK_GPM
                                       : IDS_CREDENTIAL_LEAK_TITLE_CHANGE);
}

std::u16string GetLeakDetectionTooltip() {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_LEAK_HELP_MESSAGE);
}

bool ShouldCheckPasswords(CredentialLeakType leak_type) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    return false;
  }
#endif
  return password_manager::IsPasswordUsedOnOtherSites(leak_type);
}

bool ShouldShowCancelButton(CredentialLeakType leak_type) {
  return ShouldCheckPasswords(leak_type);
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
