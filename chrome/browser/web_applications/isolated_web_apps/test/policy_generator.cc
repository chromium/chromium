// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/policy_generator.h"

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"

namespace {
base::Value::Dict AppPrefValue(std::string swb_id,
                               std::string update_manifest_url) {
  base::Value::Dict entry_dict;
  entry_dict.Set(web_app::kPolicyUpdateManifestUrlKey,
                 std::move(update_manifest_url));
  entry_dict.Set(web_app::kPolicyWebBundleIdKey, std::move(swb_id));

  return entry_dict;
}
}  // namespace

namespace web_app {

PolicyGenerator::PolicyGenerator() = default;
PolicyGenerator::~PolicyGenerator() = default;

void PolicyGenerator::AddForceInstalledIwa(web_package::SignedWebBundleId id,
                                           GURL update_manifest_url) {
  app_policies_.emplace_back(IwaForceInstalledPolicy{
      .id_ = std::move(id),
      .update_manifest_url_ = std::move(update_manifest_url)});
}

base::Value PolicyGenerator::Generate() {
  base::Value::List policy;
  for (const auto& app_policy_value : app_policies_) {
    policy.Append(AppPrefValue(app_policy_value.id_.id(),
                               app_policy_value.update_manifest_url_.spec()));
  }

  return base::Value(std::move(policy));
}

}  // namespace web_app
