// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"

#include <algorithm>

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_policy_util.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"  // nogncheck
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {

base::flat_set<web_package::SignedWebBundleId>&
GetTrustedWebBundleIdsForTesting() {
  static base::NoDestructor<base::flat_set<web_package::SignedWebBundleId>>
      trusted_web_bundle_ids_for_testing;
  return *trusted_web_bundle_ids_for_testing;
}

#if BUILDFLAG(IS_CHROMEOS)
// Returns `true` if this Web Bundle ID is configured and currently running as
// a kiosk. Kiosk mode is configured via DeviceLocalAccounts enterprise
// policy.
bool IsTrustedAsKiosk(const web_package::SignedWebBundleId& web_bundle_id) {
  return ash::GetCurrentKioskIwaBundleId() == web_bundle_id;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsTrustedViaPolicy(Profile& profile,
                        const web_package::SignedWebBundleId& web_bundle_id) {
  return std::ranges::any_of(
      profile.GetPrefs()->GetList(prefs::kIsolatedWebAppInstallForceList),
      [&web_bundle_id](const base::Value& force_install_entry) {
        auto options =
            IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
                force_install_entry);
        return options.has_value() && options->web_bundle_id() == web_bundle_id;
      });
}

}  // namespace

// static
IsolatedWebAppTrustChecker::Result IsolatedWebAppTrustChecker::IsTrusted(
    Profile& profile,
    const web_package::SignedWebBundleId& web_bundle_id,
    bool is_dev_mode_bundle) {
  if (web_bundle_id.is_for_proxy_mode()) {
    return {.status = Result::Status::kErrorUnsupportedWebBundleIdType,
            .message = "Web Bundle IDs of type ProxyMode are not supported."};
  }

  if (IsTrustedViaPolicy(profile, web_bundle_id)) {
    return {.status = Result::Status::kTrusted};
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (IsTrustedAsKiosk(web_bundle_id)) {
    return {.status = Result::Status::kTrusted};
  }

  if (ash::IsShimlessRmaAppBrowserContext(&profile) &&
      chromeos::Is3pDiagnosticsIwaId(web_bundle_id)) {
    return {.status = Result::Status::kTrusted};
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (is_dev_mode_bundle && IsIwaDevModeEnabled(&profile)) {
    return {.status = Result::Status::kTrusted};
  }

  if (GetTrustedWebBundleIdsForTesting().contains(web_bundle_id)) {
    CHECK_IS_TEST();
    return {.status = Result::Status::kTrusted};
  }

  return {.status = Result::Status::kErrorPublicKeysNotTrusted,
          .message = "The public key(s) are not trusted."};
}

void SetTrustedWebBundleIdsForTesting(  // IN-TEST
    base::flat_set<web_package::SignedWebBundleId> trusted_web_bundle_ids) {
  DCHECK(
      std::ranges::none_of(trusted_web_bundle_ids,
                           &web_package::SignedWebBundleId::is_for_proxy_mode))
      << "Cannot trust Web Bundle IDs of type ProxyMode";

  GetTrustedWebBundleIdsForTesting() =  // IN-TEST
      std::move(trusted_web_bundle_ids);
}

void AddTrustedWebBundleIdForTesting(  // IN-TEST
    const web_package::SignedWebBundleId& trusted_web_bundle_id) {
  DCHECK(!trusted_web_bundle_id.is_for_proxy_mode())
      << "Cannot trust Web Bundle IDs of type ProxyMode";

  GetTrustedWebBundleIdsForTesting().insert(trusted_web_bundle_id);  // IN-TEST
}

}  // namespace web_app
