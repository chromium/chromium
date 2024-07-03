// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_KEY_ROTATION_TEST_UTILS_H_
#define COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_KEY_ROTATION_TEST_UTILS_H_

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_rotation/iwa_key_rotation_info_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/key_rotation/proto/key_rotation.pb.h"

namespace web_app::test {

// Synchronously updates the key rotation info provider with data from `path`.
base::expected<void, IwaKeyRotationInfoProvider::ComponentUpdateError>
UpdateKeyRotationInfo(const base::Version& version, const base::FilePath& path);

// Synchronously updates the key rotation info provider with the given
// `kr_proto`.
base::expected<void, IwaKeyRotationInfoProvider::ComponentUpdateError>
UpdateKeyRotationInfo(const base::Version& version,
                      const IwaKeyRotations& kr_proto);

// Synchronously updates the key rotation info provider with a protobuf
// that maps `web_bundle_id` to `expected_key`. If `expected_key` is a nullopt,
// then the IWA with `web_bundle_id` will fail signature verification.
base::expected<void, IwaKeyRotationInfoProvider::ComponentUpdateError>
UpdateKeyRotationInfo(const base::Version& version,
                      const std::string& web_bundle_id,
                      std::optional<base::span<const uint8_t>> expected_key);

}  // namespace web_app::test

#endif  // COMPONENTS_WEB_PACKAGE_TEST_SUPPORT_SIGNED_WEB_BUNDLES_KEY_ROTATION_TEST_UTILS_H_
