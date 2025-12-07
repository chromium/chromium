// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/permission_settings_registry.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/values.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/geolocation_setting_delegate.h"
#include "components/content_settings/core/browser/permission_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"

namespace content_settings {

namespace {

base::LazyInstance<PermissionSettingsRegistry>::DestructorAtExit
    g_content_settings_registry_instance = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
PermissionSettingsRegistry* PermissionSettingsRegistry::GetInstance() {
  return g_content_settings_registry_instance.Pointer();
}

PermissionSettingsRegistry::PermissionSettingsRegistry()
    : PermissionSettingsRegistry(WebsiteSettingsRegistry::GetInstance()) {}

PermissionSettingsRegistry::PermissionSettingsRegistry(
    WebsiteSettingsRegistry* website_settings_registry)
    // This object depends on WebsiteSettingsRegistry, so get it first so that
    // they will be destroyed in reverse order.
    : website_settings_registry_(website_settings_registry) {
  Init();
}

void PermissionSettingsRegistry::ResetForTesting() {
  permission_settings_info_.clear();
  website_settings_registry_->ResetForTest();  // IN-TEST
  Init();
}

PermissionSettingsRegistry::~PermissionSettingsRegistry() = default;

const PermissionSettingsInfo* PermissionSettingsRegistry::Get(
    ContentSettingsType type) const {
  const auto& it = permission_settings_info_.find(type);
  if (it != permission_settings_info_.end()) {
    return it->second.get();
  }
  return nullptr;
}

PermissionSettingsRegistry::const_iterator PermissionSettingsRegistry::begin()
    const {
  return const_iterator(permission_settings_info_.begin());
}

PermissionSettingsRegistry::const_iterator PermissionSettingsRegistry::end()
    const {
  return const_iterator(permission_settings_info_.end());
}

void PermissionSettingsRegistry::Init() {
  // WARNING: The string names of the permissions passed in below are used to
  // generate preference names and should never be changed!
  //
  // If a permission is DELETED, please update
  // PrefProvider::DiscardOrMigrateObsoletePreferences() and
  // DefaultProvider::DiscardOrMigrateObsoletePreferences() accordingly.
    Register(ContentSettingsType::GEOLOCATION_WITH_OPTIONS,
             "geolocation-with-options",
             GeolocationSetting(PermissionOption::kAsk, PermissionOption::kAsk),
             WebsiteSettingsInfo::UNSYNCABLE,
             /*allowlisted_primary_schemes=*/{},
             WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE,
             WebsiteSettingsRegistry::PLATFORM_ANDROID |
                 WebsiteSettingsRegistry::DESKTOP,
             PermissionSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY,
             std::make_unique<GeolocationSettingDelegate>());
}

const PermissionSettingsInfo* PermissionSettingsRegistry::Register(
    ContentSettingsType type,
    const std::string& name,
    PermissionSetting initial_default_value,
    WebsiteSettingsInfo::SyncStatus sync_status,
    const std::vector<std::string>& allowlisted_primary_schemes,
    WebsiteSettingsInfo::ScopingType scoping_type,
    WebsiteSettingsRegistry::Platforms platforms,
    PermissionSettingsInfo::OriginRestriction origin_restriction,
    std::unique_ptr<PermissionSettingsInfo::Delegate> delegate) {
  // Ensure that nothing has been registered yet for the given type.
  DCHECK(!website_settings_registry_->Get(type));
  DCHECK(delegate);

  base::Value default_value = delegate->ToValue(initial_default_value);
  const WebsiteSettingsInfo* website_settings_info =
      website_settings_registry_->Register(
          type, name, std::move(default_value), sync_status,
          WebsiteSettingsInfo::NOT_LOSSY, scoping_type, platforms,
          WebsiteSettingsInfo::INHERIT_IN_INCOGNITO);

  // WebsiteSettingsInfo::Register() will return nullptr if content setting type
  // is not used on the current platform and doesn't need to be registered.
  if (!website_settings_info) {
    return nullptr;
  }

  DCHECK(!base::Contains(permission_settings_info_, type));
  auto& info = permission_settings_info_[type] =
      std::make_unique<PermissionSettingsInfo>(
          website_settings_info, allowlisted_primary_schemes,
          origin_restriction, std::move(delegate));

  return info.get();
}

}  // namespace content_settings
