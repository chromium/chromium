// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_ui_utils.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"

namespace password_manager {

namespace {

using affiliations::FacetURI;

// The URL prefixes that are removed from shown origin.
const char* const kRemovedPrefixes[] = {"m.", "mobile.", "www."};

constexpr char kPlayStoreAppPrefix[] =
    "https://play.google.com/store/apps/details?id=";

std::string GetShownOrigin(const FacetURI& facet_uri,
                           const std::string& app_display_name,
                           const GURL& url) {
  if (facet_uri.IsValidAndroidFacetURI()) {
    return app_display_name.empty() ? facet_uri.GetAndroidPackageDisplayName()
                                    : app_display_name;
  } else {
    return password_manager::GetShownOrigin(url::Origin::Create(url));
  }
}

GURL GetShownURL(const FacetURI& facet_uri, const GURL& url) {
  if (facet_uri.IsValidAndroidFacetURI()) {
    return GURL(kPlayStoreAppPrefix + facet_uri.android_package_name());
  } else {
    return url;
  }
}

}  // namespace

std::string GetShownOrigin(const CredentialUIEntry& credential) {
  FacetURI facet_uri =
      FacetURI::FromPotentiallyInvalidSpec(credential.GetFirstSignonRealm());
  return GetShownOrigin(facet_uri, credential.GetDisplayName(),
                        credential.GetURL());
}

GURL GetShownUrl(const CredentialUIEntry& credential) {
  FacetURI facet_uri =
      FacetURI::FromPotentiallyInvalidSpec(credential.GetFirstSignonRealm());
  return GetShownURL(facet_uri, credential.GetURL());
}

std::string GetShownOrigin(const url::Origin& origin) {
  std::string original =
      base::UTF16ToUTF8(url_formatter::FormatOriginForSecurityDisplay(
          origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
  std::string_view result = original;
  for (std::string_view prefix : kRemovedPrefixes) {
    if (base::StartsWith(result, prefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      result.remove_prefix(prefix.length());
      break;  // Remove only one prefix (e.g. www.mobile.de).
    }
  }

  return result.find('.') != std::string_view::npos ? std::string(result)
                                                    : original;
}

void UpdatePasswordFormUsernameAndPassword(
    const std::u16string& username,
    const std::u16string& password,
    PasswordFormManagerForUI* form_manager) {
  const auto& pending_credentials = form_manager->GetPendingCredentials();
  bool username_edited = pending_credentials.username_value != username;
  bool password_changed = pending_credentials.password_value != password;
  if (username_edited) {
    form_manager->OnUpdateUsernameFromPrompt(username);
    if (form_manager->GetMetricsRecorder()) {
      form_manager->GetMetricsRecorder()->RecordDetailedUserAction(
          password_manager::PasswordFormMetricsRecorder::DetailedUserAction::
              kEditedUsernameInBubble);
    }
  }
  if (password_changed) {
    form_manager->OnUpdatePasswordFromPrompt(password);
    if (form_manager->GetMetricsRecorder()) {
      form_manager->GetMetricsRecorder()->RecordDetailedUserAction(
          password_manager::PasswordFormMetricsRecorder::DetailedUserAction::
              kSelectedDifferentPasswordInBubble);
    }
  }

  // Values of this histogram are a bit mask. Only the lower two bits are used:
  // 0001 to indicate that the user has edited the username in the password save
  // bubble.
  // 0010 to indicate that the user has changed the password in the
  // password save bubble.
  // The maximum possible value is defined by OR-ing these values.
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.EditsInSaveBubble",
                            username_edited + 2 * password_changed, 4);
}

std::vector<std::u16string> GetUsernamesForRealm(
    const std::vector<password_manager::CredentialUIEntry>& credentials,
    const std::string& signon_realm,
    bool is_using_account_store) {
  std::vector<std::u16string> usernames;
  PasswordForm::Store store = is_using_account_store
                                  ? PasswordForm::Store::kAccountStore
                                  : PasswordForm::Store::kProfileStore;
  for (const auto& credential : credentials) {
    if (credential.GetFirstSignonRealm() == signon_realm &&
        credential.stored_in.contains(store)) {
      usernames.push_back(credential.username);
    }
  }
  return usernames;
}

std::u16string ToUsernameString(const std::u16string& username) {
  if (!username.empty()) {
    return username;
  }
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN);
}

std::u16string ToUsernameString(const std::string& username) {
  return ToUsernameString(base::UTF8ToUTF16(username));
}

}  // namespace password_manager
