// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_TRUST_CHECKER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_TRUST_CHECKER_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "components/webapps/isolated_web_apps/types/source.h"

namespace web_package {
class SignedWebBundleId;
}  // namespace web_package

class Profile;

namespace web_app {

class WebApp;

// This class is responsible for checking whether an Isolated Web App is trusted
// by the user agent for a specific operation (install/update/metadata
// reading/regular content serving). Isolated Web Apps installed in developer
// mode are only trusted if developer mode is enabled.
class IsolatedWebAppTrustChecker {
 public:
  // This oracle decides whether interacting with the IWA identified by
  // `web_bundle_id` and `dev_mode` is fine for the given `operation`;
  // operations normally include installation (managed, user or dev), updates or
  // metadata reading for populating the user install dialog.
  // This is normally supposed to be called even before the system starts
  // interacting with an IWA.
  static base::expected<void, std::string> IsOperationAllowed(
      Profile& profile,
      const web_package::SignedWebBundleId& web_bundle_id,
      bool dev_mode,
      const IwaOperation& operation);

  // This oracle is supposed to generally behave the same as
  // `IsOperationAllowed()`, but is invoked on a per-resource basis during an
  // operation (i.e. when the bundle is already downloaded and
  // manifest/icons/etc are being actively read from it).
  static base::expected<void, std::string> IsResourceLoadingAllowed(
      Profile& profile,
      const web_package::SignedWebBundleId& web_bundle_id,
      const NonInstalledBundleInspectionContext& context);

  // This oracle decides whether it's fine to load resources from an installed
  // IWA during a regular run.
  static base::expected<void, std::string> IsResourceLoadingAllowed(
      Profile& profile,
      const web_package::SignedWebBundleId& web_bundle_id,
      const WebApp& iwa);
};

// Used in tests to pretend that the given Web Bundle IDs are trusted.
void SetTrustedWebBundleIdsForTesting(
    base::flat_set<web_package::SignedWebBundleId> trusted_web_bundle_ids);

// Used in tests to pretend that a given Web Bundle ID is trusted.
void AddTrustedWebBundleIdForTesting(
    const web_package::SignedWebBundleId& trusted_web_bundle_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_TRUST_CHECKER_H_
