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
#include "components/webapps/isolated_web_apps/test_support/key_distribution/test_utils.h"

namespace web_app::test {

// Writes `kd_proto` into `DIR_COMPONENT_USER/IwaKeyDistribution/{version}` and
// triggers the registration process with the component updater. The directory
// is deleted once IwaKeyDistributionInfoProvider has processed the update
// (regardless of the outcome).
base::expected<void, IwaComponentUpdateError>
InstallIwaKeyDistributionComponent(const base::Version& version,
                                   const IwaKeyDistribution& kd_proto);

// A shortcut for the above function that populates only the key rotation part
// of the proto.
base::expected<void, IwaComponentUpdateError>
InstallIwaKeyDistributionComponent(
    const base::Version& version,
    const std::string& web_bundle_id,
    std::optional<base::span<const uint8_t>> expected_key);

// Synchronously registers the component with the component updater and waits
// for the component updater to pick up the on-disk data in its folder.
base::expected<IwaComponentMetadata, IwaComponentUpdateError>
RegisterIwaKeyDistributionComponentAndWaitForLoad();

}  // namespace web_app::test

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_TEST_KEY_DISTRIBUTION_TEST_UTILS_H_
