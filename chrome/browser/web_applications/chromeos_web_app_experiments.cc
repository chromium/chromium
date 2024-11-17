// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"

#include <string_view>

#include "ash/constants/web_app_id_constants.h"
#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace web_app {

namespace {

bool g_always_enabled_for_testing = false;

bool IsExperimentEnabled(const webapps::AppId& app_id) {
  return g_always_enabled_for_testing || app_id == ash::kMicrosoft365AppId;
}

// IsValidScopeExtenion returns whether a url can be successfully turned into
// a scope extension or not.
bool IsValidScopeExtension(const GURL& url) {
  return url.is_valid() && url.IsStandard() && url.has_host() &&
         !base::StartsWith(url.host(), ".");
}

std::vector<std::string> GetListFromFinchParam(const std::string& finch_param) {
  return base::SplitString(finch_param, ",",
                           base::WhitespaceHandling::TRIM_WHITESPACE,
                           base::SplitResult::SPLIT_WANT_NONEMPTY);
}

std::optional<std::vector<const char*>>&
GetScopeExtensionsOverrideForTesting() {
  static base::NoDestructor<std::optional<std::vector<const char*>>>
      scope_extensions;
  return *scope_extensions;
}

}  // namespace

ScopeExtensions ChromeOsWebAppExperiments::GetScopeExtensions(
    const webapps::AppId& app_id) {
  DCHECK(chromeos::features::IsUploadOfficeToCloudEnabled());

  ScopeExtensions extensions;
  if (!IsExperimentEnabled(app_id)) {
    return extensions;
  }

  if (GetScopeExtensionsOverrideForTesting()) {
    for (const auto* origin : *GetScopeExtensionsOverrideForTesting()) {
      extensions.insert(
          ScopeExtensionInfo{.origin = url::Origin::Create(GURL(origin))});
    }
    return extensions;
  }

  const auto microsoft365_scope_extension_urls = GetListFromFinchParam(
      chromeos::features::kMicrosoft365ScopeExtensionsURLs.Get());
  for (const auto& url_string : microsoft365_scope_extension_urls) {
    const GURL url = GURL(url_string);
    if (!IsValidScopeExtension(url)) {
      LOG(WARNING) << "Skipping invalid M365 scope extension URL from Finch: "
                   << url_string;
      continue;
    }
    extensions.insert(
        ScopeExtensionInfo{.origin = url::Origin::Create(GURL(url))});
  }
  const auto microsoft365_scope_extension_domains = GetListFromFinchParam(
      chromeos::features::kMicrosoft365ScopeExtensionsDomains.Get());
  for (const auto& url_string : microsoft365_scope_extension_domains) {
    const GURL url = GURL(url_string);
    if (!IsValidScopeExtension(url)) {
      LOG(WARNING)
          << "Skipping invalid M365 scope extension domain from Finch: "
          << url_string;
      continue;
    }
    extensions.insert(ScopeExtensionInfo{
        .origin = url::Origin::Create(GURL(url)), .has_origin_wildcard = true});
  }
  return extensions;
}

bool ChromeOsWebAppExperiments::ShouldAddLinkPreference(
    const webapps::AppId& app_id,
    Profile* profile) {
  return IsExperimentEnabled(app_id) &&
         chromeos::cloud_upload::IsMicrosoftOfficeOneDriveIntegrationAutomated(
             profile);
}

int ChromeOsWebAppExperiments::GetExtendedScopeScore(
    const webapps::AppId& app_id,
    std::string_view url_spec) {
  DCHECK(chromeos::features::IsUploadOfficeToCloudEnabled());

  const GURL url = GURL(url_spec);
  const auto extensions = GetScopeExtensions(app_id);
  int best_score = 0;
  for (const ScopeExtensionInfo& scope : extensions) {
    const GURL scope_origin = scope.origin.GetURL();
    int score;
    if (scope.has_origin_wildcard) {
      score =
          url.DomainIs(scope_origin.host()) ? scope_origin.spec().length() : 0;
    } else {
      score = base::StartsWith(url_spec, scope_origin.spec(),
                               base::CompareCase::SENSITIVE)
                  ? scope_origin.spec().length()
                  : 0;
    }
    best_score = std::max(best_score, score);
  }
  return best_score;
}

bool ChromeOsWebAppExperiments::IgnoreManifestColor(
    const webapps::AppId& app_id) {
  DCHECK(chromeos::features::IsUploadOfficeToCloudEnabled());
  return IsExperimentEnabled(app_id);
}

bool ChromeOsWebAppExperiments::IsNavigationCapturingReimplEnabledForTargetApp(
    const webapps::AppId& target_app_id) {
  return ::chromeos::features::IsOfficeNavigationCapturingReimplEnabled() &&
         IsExperimentEnabled(target_app_id);
}

bool ChromeOsWebAppExperiments::IsNavigationCapturingReimplEnabledForSourceApp(
    const webapps::AppId& source_app_id,
    const GURL& url) {
  // Until Navigation Capturing Reimplementation is fully enabled, hardcode
  // specific destination URLs for the typical scenarios in which we want the
  // user to stay inside the Office PWA (note that URLs that are already within
  // the PWA's scope are covered by
  // `IsNavigationCapturingReimplEnabledForTargetApp()`).
  return ::chromeos::features::IsOfficeNavigationCapturingReimplEnabled() &&
         IsExperimentEnabled(source_app_id) && url == url::kAboutBlankURL;
}

bool ChromeOsWebAppExperiments::ShouldLaunchForRedirectedNavigation(
    const webapps::AppId& target_app_id) {
  return IsExperimentEnabled(target_app_id);
}

void ChromeOsWebAppExperiments::SetAlwaysEnabledForTesting() {
  g_always_enabled_for_testing = true;
}

void ChromeOsWebAppExperiments::SetScopeExtensionsForTesting(
    std::vector<const char*> scope_extensions_override) {
  GetScopeExtensionsOverrideForTesting() = std::move(scope_extensions_override);
}

void ChromeOsWebAppExperiments::ClearOverridesForTesting() {
  g_always_enabled_for_testing = false;
  GetScopeExtensionsOverrideForTesting().reset();
}

}  // namespace web_app
