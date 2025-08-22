// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_TEST_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_TEST_UTILS_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/values.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "url/gurl.h"

class PrefService;

namespace web_app::test {

// Appends `policy_entry` directly to `prefs::kIsolatedWebAppInstallForceList`
// in order to force-install the IWA. Doesn't remove existing values.
void AddForceInstalledIwaToPolicy(PrefService* prefs,
                                  base::Value::Dict policy_entry);

// Removes the policy entry associated with the given `web_bundle_id` from
// `prefs::kIsolatedWebAppInstallForceList`.
void RemoveForceInstalledIwaFromPolicy(
    PrefService* prefs,
    const web_package::SignedWebBundleId& web_bundle_id);

// Edits the policy entry associated with the given `web_bundle_id` in
// `prefs::kIsolatedWebAppInstallForceList`.
void EditForceInstalledIwaPolicy(
    PrefService* prefs,
    const web_package::SignedWebBundleId& web_bundle_id,
    base::Value::Dict policy_entry);

// Generates a policy entry that can be appended to
// `prefs::kIsolatedWebAppInstallForceList` in order to force-install the IWA.
base::Value::Dict CreateForceInstallIwaPolicyEntry(
    const web_package::SignedWebBundleId& web_bundle_id,
    const GURL& update_manifest_url,
    const std::optional<UpdateChannel>& update_channel = std::nullopt,
    const std::optional<IwaVersion>& pinned_version = std::nullopt,
    bool allow_downgrades = false);

// Generates a policy entry that can be appended to
// `prefs::kIsolatedWebAppInstallForceList` in order to force-install the IWA.
// Doesn't perform any sanity checks on the provided values. Should only be used
// for failure scenarios.
base::Value::Dict CreateForceInstallIwaPolicyEntry(
    std::string_view web_bundle_id,
    std::string_view update_manifest_url,
    const std::optional<std::string>& update_channel = std::nullopt,
    const std::optional<std::string>& pinned_version = std::nullopt,
    bool allow_downgrades = false);

}  // namespace web_app::test

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_TEST_UTILS_H_
