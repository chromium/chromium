// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_TRUST_CHECKER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_TRUST_CHECKER_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"

namespace web_package {
class SignedWebBundleId;
class SignedWebBundleIntegrityBlock;
}  // namespace web_package

class PrefService;

namespace web_app {

// This class is responsible for checking whether an Isolated Web App is signed
// by parties trusted by the user agent. The user agent will only run trusted
// Isolated Web Apps, and refuse to run untrusted ones, except when Isolated Web
// App developer mode is enabled.
//
// "Trusting" an Isolated Web App means that the public keys of the Signed Web
// Bundle's Integrity Block, in combination with the app's expected Web Bundle
// ID, are trusted. This class only checks whether the keys are trusted, and
// does not verify the signatures themselves.
//
// An Isolated Web App is trusted in the following scenarios:
// 1. The Web Bundle ID of an Isolated Web App is configured via enterprise
//    policy to be trusted.
// 2. Isolated Web App developer mode (`features::kIsolatedWebAppDevMode`) is
//    enabled. This is used by developers to test their Isolated Web Apps during
//    development.
// 3. [Only in Tests] The Web Bundle ID of an Isolated Web App is configured as
//    trusted via a call to `SetTrustedWebBundleIdsForTesting`.
//
// In the near future, we will also add support for trusting a list of public
// keys from trusted partners.
//
// In the longer term future, we will also add support for trusting Isolated Web
// Apps that were countersigned by a trusted distributor/store.
class IsolatedWebAppTrustChecker {
 public:
  explicit IsolatedWebAppTrustChecker(const PrefService& pref_service);

  virtual ~IsolatedWebAppTrustChecker();

  struct Result;

  // Checks whether the user agent trusts the Isolated Web App identified by the
  // `expected_web_bundle_id` and the given `public_key_stack`. Returns with
  // `Result::Type::kTrusted` if the Isolated Web App is trusted.
  //
  // This function also makes sure that the `expected_web_bundle_id` is actually
  // derived from the `public_key_stack`.
  //
  // Important: This method does not verify the signatures themselves - it only
  // checks whether the public keys associated with these signatures correspond
  // to trusted parties.
  virtual Result IsTrusted(
      const web_package::SignedWebBundleId& expected_web_bundle_id,
      const web_package::SignedWebBundleIntegrityBlock& integrity_block) const;

  struct Result {
    enum class Status {
      kTrusted,
      kErrorUnsupportedWebBundleIdType,
      kErrorInvalidSignatureStackLength,
      kErrorWebBundleIdNotDerivedFromFirstPublicKey,
      kErrorPublicKeysNotTrusted,
    };

    Status status;
    std::string message;
  };

 private:
#if BUILDFLAG(IS_CHROMEOS)
  // Returns `true` if trust for this Web Bundle ID is established via
  // enterprise policy.
  [[nodiscard]] bool IsTrustedViaPolicy(
      const web_package::SignedWebBundleId& web_bundle_id) const;
#endif

  // Returns `true` if trust for this Web Bundle ID is established because
  // Isolated Web App developer mode is enabled.
  [[nodiscard]] bool IsTrustedViaDevMode(
      const web_package::SignedWebBundleId& web_bundle_id) const;

  base::raw_ref<const PrefService> pref_service_;

  base::WeakPtrFactory<IsolatedWebAppTrustChecker> weak_ptr_factory_{this};
};

// Used in tests to pretend that the given Web Bundle IDs are trusted.
void SetTrustedWebBundleIdsForTesting(
    base::flat_set<web_package::SignedWebBundleId> trusted_web_bundle_ids);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_TRUST_CHECKER_H_
