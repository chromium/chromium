// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"

#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/common/content_features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#endif

namespace web_app {

namespace {

base::flat_set<web_package::SignedWebBundleId>&
GetTrustedWebBundleIdsForTesting() {
  static base::NoDestructor<base::flat_set<web_package::SignedWebBundleId>>
      trusted_web_bundle_ids_for_testing;
  return *trusted_web_bundle_ids_for_testing;
}

}  // namespace

IsolatedWebAppTrustChecker::IsolatedWebAppTrustChecker(Profile& profile)
    : profile_(profile) {}

IsolatedWebAppTrustChecker::~IsolatedWebAppTrustChecker() = default;

IsolatedWebAppTrustChecker::Result IsolatedWebAppTrustChecker::IsTrusted(
    const web_package::SignedWebBundleId& web_bundle_id,
    bool is_dev_mode_bundle) const {
  if (web_bundle_id.is_for_proxy_mode()) {
    return {.status = Result::Status::kErrorUnsupportedWebBundleIdType,
            .message = "Web Bundle IDs of type ProxyMode are not supported."};
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (IsTrustedViaPolicy(web_bundle_id)) {
    return {.status = Result::Status::kTrusted};
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (is_dev_mode_bundle && IsIwaDevModeEnabled(&*profile_)) {
    return {.status = Result::Status::kTrusted};
  }

  if (GetTrustedWebBundleIdsForTesting().contains(web_bundle_id)) {
    CHECK_IS_TEST();
    return {.status = Result::Status::kTrusted};
  }

  return {.status = Result::Status::kErrorPublicKeysNotTrusted,
          .message = "The public key(s) are not trusted."};
}

#if BUILDFLAG(IS_CHROMEOS)
bool IsolatedWebAppTrustChecker::IsTrustedViaPolicy(
    const web_package::SignedWebBundleId& web_bundle_id) const {
  const PrefService::Preference* pref = profile_->GetPrefs()->FindPreference(
      prefs::kIsolatedWebAppInstallForceList);
  if (!pref) {
    NOTREACHED();
    return false;
  }

  return base::ranges::any_of(
      pref->GetValue()->GetList(),
      [&web_bundle_id](const base::Value& force_install_entry) {
        auto options =
            IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
                force_install_entry);
        return options.has_value() && options->web_bundle_id() == web_bundle_id;
      });
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void SetTrustedWebBundleIdsForTesting(  // IN-TEST
    base::flat_set<web_package::SignedWebBundleId> trusted_web_bundle_ids) {
  DCHECK(
      base::ranges::none_of(trusted_web_bundle_ids,
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
