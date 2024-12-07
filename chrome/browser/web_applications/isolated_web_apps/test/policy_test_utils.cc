// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace web_app::test {

// Appends `policy_entry` directly to `prefs::kIsolatedWebAppInstallForceList`
// in order to force-install the IWA. Doesn't remove existing values.
void AddForceInstalledIwaToPolicy(PrefService* prefs,
                                  base::Value::Dict policy_entry) {
  ScopedListPrefUpdate update{prefs, prefs::kIsolatedWebAppInstallForceList};
  update->Append(std::move(policy_entry));
  // RAII applies the update.
}

// Generates a policy entry that can be appended to
// `prefs::kIsolatedWebAppInstallForceList` in order to force-install the IWA.
base::Value::Dict CreateForceInstallIwaPolicyEntry(
    const web_package::SignedWebBundleId& web_bundle_id,
    const GURL& update_manifest_url,
    const std::optional<UpdateChannel>& update_channel,
    const std::optional<base::Version>& pinned_version,
    bool allow_downgrades) {
  return CreateForceInstallIwaPolicyEntry(
      web_bundle_id.id(), update_manifest_url.spec(),
      update_channel ? std::make_optional(update_channel->ToString())
                     : std::nullopt,
      pinned_version ? std::make_optional(pinned_version->GetString())
                     : std::nullopt,
      allow_downgrades);
}

// Generates a policy entry that can be appended to
// `prefs::kIsolatedWebAppInstallForceList` in order to force-install the IWA.
base::Value::Dict CreateForceInstallIwaPolicyEntry(
    std::string_view web_bundle_id,
    std::string_view update_manifest_url,
    const std::optional<std::string>& update_channel,
    const std::optional<std::string>& pinned_version,
    bool allow_downgrades) {
  // allow_downgrades cannot be toggled on without specifying pinned_version
  // field.
  CHECK(!allow_downgrades || pinned_version);

  base::Value::Dict policy_entry =
      base::Value::Dict()
          .Set(kPolicyWebBundleIdKey, web_bundle_id)
          .Set(kPolicyUpdateManifestUrlKey, update_manifest_url)
          .Set(kPolicyAllowDowngradesKey, allow_downgrades);

  if (update_channel) {
    policy_entry.Set(kPolicyUpdateChannelKey, *update_channel);
  }

  if (pinned_version) {
    policy_entry.Set(kPolicyPinnedVersionKey, *pinned_version);
  }

  return policy_entry;
}

}  // namespace web_app::test
