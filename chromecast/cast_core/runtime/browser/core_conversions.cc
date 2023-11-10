// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/core_conversions.h"

#include "base/ranges/algorithm.h"
#include "chromecast/common/feature_constants.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/cast_core/public/src/proto/common/application_config.pb.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace chromecast {
namespace {

std::vector<blink::PermissionType> GetFeaturePermissions(
    const cast::common::ApplicationConfig& core_config) {
  std::vector<blink::PermissionType> feature_permissions;
  auto it = base::ranges::find(core_config.extra_features().entries(),
                               feature::kCastCoreFeaturePermissions,
                               &cast::common::Dictionary::Entry::key);
  if (it == core_config.extra_features().entries().end()) {
    return feature_permissions;
  }

  CHECK(it->value().value_case() == cast::common::Value::kArray);
  base::ranges::for_each(
      it->value().array().values(),
      [&feature_permissions](const cast::common::Value& value) {
        CHECK(value.value_case() == cast::common::Value::kNumber);
        // TODO(crbug.com/1383326): Ensure this is a valid permission from an
        // allow list supported by the cast_receiver component.
        feature_permissions.push_back(
            static_cast<blink::PermissionType>(value.number()));
      });
  return feature_permissions;
}

std::vector<url::Origin> GetAdditionalFeaturePermissionOrigins(
    const cast::common::ApplicationConfig& core_config) {
  std::vector<url::Origin> feature_permission_origins;
  auto it = base::ranges::find(core_config.extra_features().entries(),
                               feature::kCastCoreFeaturePermissionOrigins,
                               &cast::common::Dictionary::Entry::key);
  if (it == core_config.extra_features().entries().end()) {
    return feature_permission_origins;
  }

  CHECK(it->value().value_case() == cast::common::Value::kArray);
  base::ranges::for_each(
      it->value().array().values(),
      [&feature_permission_origins](const cast::common::Value& value) {
        CHECK(value.value_case() == cast::common::Value::kText);
        auto origin = url::Origin::Create(GURL(value.text()));
        CHECK(!origin.opaque());
        feature_permission_origins.push_back(std::move(origin));
      });
  return feature_permission_origins;
}

cast_receiver::ApplicationConfig::ContentPermissions ToReceiverPermissions(
    const cast::common::ApplicationConfig& core_config) {
  return cast_receiver::ApplicationConfig::ContentPermissions{
      GetFeaturePermissions(core_config),
      GetAdditionalFeaturePermissionOrigins(core_config)};
}

}  // namespace

cast_receiver::ApplicationConfig ToReceiverConfig(
    const cast::common::ApplicationConfig& core_config) {
  cast_receiver::ApplicationConfig config{core_config.app_id(),
                                          core_config.display_name(),
                                          ToReceiverPermissions(core_config)};
  if (core_config.has_cast_web_app_config()) {
    config.url = GURL(core_config.cast_web_app_config().url());
    if (!config.url->is_valid()) {
      config.url = std::nullopt;
    }
  }

  return config;
}

}  // namespace chromecast
