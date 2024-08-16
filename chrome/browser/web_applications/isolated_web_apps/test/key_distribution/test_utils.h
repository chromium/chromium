// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_KEY_DISTRIBUTION_TEST_UTILS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_KEY_DISTRIBUTION_TEST_UTILS_H_

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_distribution/proto/key_distribution.pb.h"

namespace web_app::test {

// Synchronously updates the key distribution info provider with data from
// `path`.
base::expected<void, IwaKeyDistributionInfoProvider::ComponentUpdateError>
UpdateKeyDistributionInfo(const base::Version& version,
                          const base::FilePath& path);

// Synchronously updates the key distribution info provider with the given
// `kd_proto`.
base::expected<void, IwaKeyDistributionInfoProvider::ComponentUpdateError>
UpdateKeyDistributionInfo(const base::Version& version,
                          const IwaKeyDistribution& kd_proto);

// Synchronously updates the key distribution info provider with a protobuf
// that maps `web_bundle_id` to `expected_key`. If `expected_key` is a nullopt,
// then the IWA with `web_bundle_id` will fail signature verification.
base::expected<void, IwaKeyDistributionInfoProvider::ComponentUpdateError>
UpdateKeyDistributionInfo(
    const base::Version& version,
    const std::string& web_bundle_id,
    std::optional<base::span<const uint8_t>> expected_key);

// Writes `kd_proto` into `DIR_COMPONENT_USER/IwaKeyDistribution/{version}` and
// triggers the registration process with the component updater. The directory
// is deleted once IwaKeyDistributionInfoProvider has processed the update
// (regardless of the outcome).
base::expected<void, IwaKeyDistributionInfoProvider::ComponentUpdateError>
InstallIwaKeyDistributionComponent(const base::Version& version,
                                   const IwaKeyDistribution& kd_proto);

// A shortcut for the above function that populates only the key rotation part
// of the proto.
base::expected<void, IwaKeyDistributionInfoProvider::ComponentUpdateError>
InstallIwaKeyDistributionComponent(
    const base::Version& version,
    const std::string& web_bundle_id,
    std::optional<base::span<const uint8_t>> expected_key);

}  // namespace web_app::test

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_KEY_DISTRIBUTION_TEST_UTILS_H_
