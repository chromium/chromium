// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/public/application_config.h"

namespace cast_receiver {

ApplicationConfig::ContentPermissions::ContentPermissions() = default;

ApplicationConfig::ContentPermissions::ContentPermissions(
    std::vector<blink::PermissionType> permissions_set,
    std::vector<url::Origin> origins)
    : permissions(std::move(permissions_set)),
      additional_origins(std::move(origins)) {}

ApplicationConfig::ContentPermissions::~ContentPermissions() = default;

ApplicationConfig::ContentPermissions::ContentPermissions(
    const ContentPermissions& other) = default;

ApplicationConfig::ContentPermissions::ContentPermissions(
    ContentPermissions&& other) = default;

ApplicationConfig::ContentPermissions&
ApplicationConfig::ContentPermissions::operator=(
    const ContentPermissions& other) = default;

ApplicationConfig::ContentPermissions&
ApplicationConfig::ContentPermissions::operator=(ContentPermissions&& other) =
    default;

ApplicationConfig::ApplicationConfig() = default;

ApplicationConfig::ApplicationConfig(std::string id,
                                     std::string name,
                                     ContentPermissions content_permissions)
    : app_id(std::move(id)),
      display_name(std::move(name)),
      permissions(std::move(content_permissions)) {}

ApplicationConfig::~ApplicationConfig() = default;

ApplicationConfig::ApplicationConfig(const ApplicationConfig& config) = default;

ApplicationConfig::ApplicationConfig(ApplicationConfig&& config) = default;

ApplicationConfig& ApplicationConfig::operator=(
    const ApplicationConfig& config) = default;

ApplicationConfig& ApplicationConfig::operator=(ApplicationConfig&& config) =
    default;

}  // namespace cast_receiver
