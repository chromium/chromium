// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_KEY_ROTATION_KEY_ROTATION_INFO_PROVIDER_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_KEY_ROTATION_KEY_ROTATION_INFO_PROVIDER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/types/strong_alias.h"
#include "base/version.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_package {

class KeyRotationInfoProvider {
 public:
  KeyRotationInfoProvider(const KeyRotationInfoProvider&) = delete;
  KeyRotationInfoProvider& operator=(const KeyRotationInfoProvider&) = delete;

  using ExpectedKey =
      base::StrongAlias<class ExpectedKeyTag, std::vector<uint8_t>>;
  struct KeyNotFoundTag {};
  struct KeyDisabledTag {};

  // Looks up the expected key for `web_bundle_id` with respect to the external
  // data.
  using KeyLookupResult =
      absl::variant<ExpectedKey, KeyDisabledTag, KeyNotFoundTag>;
  virtual KeyLookupResult GetExpectedSigningKey(
      std::string_view web_bundle_id) const = 0;

 protected:
  KeyRotationInfoProvider() = default;
  virtual ~KeyRotationInfoProvider() = default;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_KEY_ROTATION_KEY_ROTATION_INFO_PROVIDER_H_
