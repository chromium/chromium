// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"

namespace web_app::test {

// Appends `policy_entry` directly to `prefs::kIsolatedWebAppInstallForceList`
// in order to force-install the IWA. Doesn't remove existing values.
void AddForceInstalledIwaToPolicy(PrefService* prefs,
                                  base::Value::Dict policy_entry) {
  ScopedListPrefUpdate update{prefs, prefs::kIsolatedWebAppInstallForceList};
  update->Append(std::move(policy_entry));
  // RAII applies the update.
}

// Removes the policy entry associated with the given `web_bundle_id` from
// `prefs::kIsolatedWebAppInstallForceList`.
void RemoveForceInstalledIwaFromPolicy(
    PrefService* prefs,
    const web_package::SignedWebBundleId& web_bundle_id) {
  ScopedListPrefUpdate update{prefs, prefs::kIsolatedWebAppInstallForceList};
  update->EraseIf([&web_bundle_id](const base::Value& entry) {
    const std::string* id = entry.GetDict().FindString(kPolicyWebBundleIdKey);
    return id && *id == web_bundle_id.id();
  });
}

// Edits the policy entry associated with the given `web_bundle_id` in
// `prefs::kIsolatedWebAppInstallForceList`.
void EditForceInstalledIwaPolicy(
    PrefService* prefs,
    const web_package::SignedWebBundleId& web_bundle_id,
    base::Value::Dict policy_entry) {
  ScopedListPrefUpdate update{prefs, prefs::kIsolatedWebAppInstallForceList};
  auto itr =
      std::ranges::find(*update, web_bundle_id.id(), [](const auto& entry) {
        return *entry.GetDict().FindString(kPolicyWebBundleIdKey);
      });
  CHECK(itr != update->end());
  itr->GetDict() = std::move(policy_entry);
}

// Generates a policy entry that can be appended to
// `prefs::kIsolatedWebAppInstallForceList` in order to force-install the IWA.
base::Value::Dict CreateForceInstallIwaPolicyEntry(
    const web_package::SignedWebBundleId& web_bundle_id,
    const GURL& update_manifest_url,
    const std::optional<UpdateChannel>& update_channel,
    const std::optional<IwaVersion>& pinned_version,
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
