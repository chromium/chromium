// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/test/policy_generator.h"

#include "chrome/browser/web_applications/isolated_web_apps/test/policy_test_utils.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"

namespace web_app {

PolicyGenerator::PolicyGenerator() = default;
PolicyGenerator::~PolicyGenerator() = default;

void PolicyGenerator::AddForceInstalledIwa(
    const web_package::SignedWebBundleId& web_bundle_id,
    const GURL& update_manifest_url,
    const std::optional<UpdateChannel>& channel,
    const std::optional<IwaVersion>& pinned_version,
    bool allow_downgrades) {
  app_policies_.Append(test::CreateForceInstallIwaPolicyEntry(
      web_bundle_id, update_manifest_url,
      channel.value_or(UpdateChannel::default_channel()), pinned_version,
      allow_downgrades));
}

base::Value PolicyGenerator::Generate() {
  return base::Value(app_policies_.Clone());
}

}  // namespace web_app
