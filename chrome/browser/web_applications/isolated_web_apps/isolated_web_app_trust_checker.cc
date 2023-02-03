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
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"
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

IsolatedWebAppTrustChecker::IsolatedWebAppTrustChecker(
    const PrefService& pref_service)
    : pref_service_(pref_service) {}

IsolatedWebAppTrustChecker::~IsolatedWebAppTrustChecker() = default;

IsolatedWebAppTrustChecker::Result IsolatedWebAppTrustChecker::IsTrusted(
    const web_package::SignedWebBundleId& expected_web_bundle_id,
    const web_package::SignedWebBundleIntegrityBlock& integrity_block) const {
  if (expected_web_bundle_id.type() !=
      web_package::SignedWebBundleId::Type::kEd25519PublicKey) {
    return {.status = Result::Status::kErrorUnsupportedWebBundleIdType,
            .message =
                "Only Web Bundle IDs of type Ed25519PublicKey are supported."};
  }

  if (integrity_block.signature_stack().size() != 1) {
    // TODO(crbug.com/1366303): Support more than one signature.
    return {.status = Result::Status::kErrorInvalidSignatureStackLength,
            .message =
                base::StringPrintf("Expected exactly 1 signature, but got %zu.",
                                   integrity_block.signature_stack().size())};
  }

  auto derived_web_bundle_id =
      integrity_block.signature_stack().derived_web_bundle_id();
  if (derived_web_bundle_id != expected_web_bundle_id) {
    return {
        .status = Result::Status::kErrorWebBundleIdNotDerivedFromFirstPublicKey,
        .message = base::StringPrintf(
            "The Web Bundle ID (%s) derived from the public key does not "
            "match the expected Web Bundle ID (%s).",
            derived_web_bundle_id.id().c_str(),
            expected_web_bundle_id.id().c_str())};
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (IsTrustedViaPolicy(expected_web_bundle_id)) {
    return {.status = Result::Status::kTrusted};
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (IsTrustedViaDevMode(expected_web_bundle_id)) {
    return {.status = Result::Status::kTrusted};
  }

  if (GetTrustedWebBundleIdsForTesting().contains(expected_web_bundle_id)) {
    CHECK_IS_TEST();
    return {.status = Result::Status::kTrusted};
  }

  return {.status = Result::Status::kErrorPublicKeysNotTrusted,
          .message = "The public key(s) are not trusted."};
}

#if BUILDFLAG(IS_CHROMEOS)
bool IsolatedWebAppTrustChecker::IsTrustedViaPolicy(
    const web_package::SignedWebBundleId& web_bundle_id) const {
  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kIsolatedWebAppInstallForceList);
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

bool IsolatedWebAppTrustChecker::IsTrustedViaDevMode(
    const web_package::SignedWebBundleId& web_bundle_id) const {
  return base::FeatureList::IsEnabled(features::kIsolatedWebAppDevMode);
}

void SetTrustedWebBundleIdsForTesting(  // IN-TEST
    base::flat_set<web_package::SignedWebBundleId> trusted_web_bundle_ids) {
  DCHECK(base::ranges::all_of(
      trusted_web_bundle_ids,
      [](const web_package::SignedWebBundleId& web_bundle_id) {
        return web_bundle_id.type() ==
               web_package::SignedWebBundleId::Type::kEd25519PublicKey;
      }))
      << "Can only trust Web Bundle IDs of type Ed25519PublicKey";

  GetTrustedWebBundleIdsForTesting() =  // IN-TEST
      std::move(trusted_web_bundle_ids);
}

}  // namespace web_app
