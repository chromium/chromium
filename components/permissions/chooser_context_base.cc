// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/chooser_context_base.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "url/origin.h"

namespace permissions {

const char kObjectListKey[] = "chosen-objects";

ChooserContextBase::ChooserContextBase(
    const ContentSettingsType guard_content_settings_type,
    const ContentSettingsType data_content_settings_type,
    HostContentSettingsMap* host_content_settings_map)
    : guard_content_settings_type_(guard_content_settings_type),
      data_content_settings_type_(data_content_settings_type),
      host_content_settings_map_(host_content_settings_map) {
  DCHECK(host_content_settings_map_);
}

ChooserContextBase::~ChooserContextBase() = default;

ChooserContextBase::Object::Object(const url::Origin& origin,
                                   base::Value value,
                                   content_settings::SettingSource source,
                                   bool incognito)
    : origin(origin.GetURL()),
      value(std::move(value)),
      source(source),
      incognito(incognito) {}

ChooserContextBase::Object::~Object() = default;

void ChooserContextBase::PermissionObserver::OnChooserObjectPermissionChanged(
    ContentSettingsType data_content_settings_type,
    ContentSettingsType guard_content_settings_type) {}

void ChooserContextBase::PermissionObserver::OnPermissionRevoked(
    const url::Origin& origin) {}

void ChooserContextBase::AddObserver(PermissionObserver* observer) {
  permission_observer_list_.AddObserver(observer);
}

void ChooserContextBase::RemoveObserver(PermissionObserver* observer) {
  permission_observer_list_.RemoveObserver(observer);
}

bool ChooserContextBase::CanRequestObjectPermission(const url::Origin& origin) {
  ContentSetting content_setting =
      host_content_settings_map_->GetContentSetting(
          origin.GetURL(), GURL(), guard_content_settings_type_);
  DCHECK(content_setting == CONTENT_SETTING_ASK ||
         content_setting == CONTENT_SETTING_BLOCK);
  return content_setting == CONTENT_SETTING_ASK;
}

std::unique_ptr<ChooserContextBase::Object>
ChooserContextBase::GetGrantedObject(const url::Origin& origin,
                                     const base::StringPiece key) {
  if (!CanRequestObjectPermission(origin))
    return nullptr;

  content_settings::SettingInfo info;
  base::Value setting = GetWebsiteSetting(origin, &info);

  base::Value* objects = setting.FindListKey(kObjectListKey);
  if (!objects)
    return nullptr;

  for (auto& object : objects->GetList()) {
    if (IsValidObject(object) && GetKeyForObject(object) == key) {
      return std::make_unique<Object>(
          origin, std::move(object), info.source,
          host_content_settings_map_->IsOffTheRecord());
    }
  }
  return nullptr;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
ChooserContextBase::GetGrantedObjects(const url::Origin& origin) {
  if (!CanRequestObjectPermission(origin))
    return {};

  content_settings::SettingInfo info;
  base::Value setting = GetWebsiteSetting(origin, &info);

  base::Value* objects = setting.FindListKey(kObjectListKey);
  if (!objects)
    return {};

  std::vector<std::unique_ptr<Object>> results;
  for (auto& object : objects->GetList()) {
    if (IsValidObject(object)) {
      results.push_back(std::make_unique<Object>(
          origin, std::move(object), info.source,
          host_content_settings_map_->IsOffTheRecord()));
    }
  }
  return results;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
ChooserContextBase::GetAllGrantedObjects() {
  ContentSettingsForOneType content_settings;
  host_content_settings_map_->GetSettingsForOneType(data_content_settings_type_,
                                                    &content_settings);

  std::vector<std::unique_ptr<Object>> results;
  for (const ContentSettingPatternSource& content_setting : content_settings) {
    // Old settings used the (requesting,embedding) pair whereas the new
    // settings simply use (embedding, *). The migration logic in
    // HostContentSettingsMap::MigrateSettingsPrecedingPermissionDelegationActivation
    // ensures that there is no way for leftover old settings to make us pick
    // the wrong pattern here.
    GURL origin_url(content_setting.primary_pattern.ToString());

    if (!origin_url.is_valid())
      continue;

    const auto origin = url::Origin::Create(origin_url);
    if (!CanRequestObjectPermission(origin))
      continue;

    content_settings::SettingInfo info;
    base::Value setting = GetWebsiteSetting(origin, &info);
    base::Value* objects = setting.FindListKey(kObjectListKey);
    if (!objects)
      continue;

    for (auto& object : objects->GetList()) {
      if (!IsValidObject(object)) {
        continue;
      }

      results.push_back(std::make_unique<Object>(
          origin, std::move(object), info.source, content_setting.incognito));
    }
  }

  return results;
}

void ChooserContextBase::GrantObjectPermission(const url::Origin& origin,
                                               base::Value object) {
  DCHECK(IsValidObject(object));

  base::Value setting = GetWebsiteSetting(origin, /*info=*/nullptr);
  base::Value* objects = setting.FindListKey(kObjectListKey);
  if (!objects) {
    objects =
        setting.SetKey(kObjectListKey, base::Value(base::Value::Type::LIST));
  }

  const std::string key = GetKeyForObject(object);

  if (key.empty()) {
    // Use the legacy behavior.
    if (!base::Contains(objects->GetList(), object))
      objects->Append(std::move(object));
  } else {
    base::Value::ListView object_list = objects->GetList();
    auto it = base::ranges::find_if(object_list, [this, &key](auto& obj) {
      return IsValidObject(obj) && GetKeyForObject(obj) == key;
    });
    if (it != object_list.end()) {
      // Update object permission.
      *it = std::move(object);
    } else {
      // Grant object permission.
      objects->Append(std::move(object));
    }
  }

  SetWebsiteSetting(origin, std::move(setting));
  NotifyPermissionChanged();
}

void ChooserContextBase::UpdateObjectPermission(const url::Origin& origin,
                                                const base::Value& old_object,
                                                base::Value new_object) {
  base::Value setting = GetWebsiteSetting(origin, /*info=*/nullptr);
  base::Value* objects = setting.FindListKey(kObjectListKey);
  if (!objects)
    return;

  base::Value::ListView object_list = objects->GetList();
  auto it = std::find(object_list.begin(), object_list.end(), old_object);
  if (it == object_list.end())
    return;

  *it = std::move(new_object);
  SetWebsiteSetting(origin, std::move(setting));
  NotifyPermissionChanged();
}

void ChooserContextBase::RevokeObjectPermission(const url::Origin& origin,
                                                const base::Value& object) {
  DCHECK(IsValidObject(object));

  base::Value setting = GetWebsiteSetting(origin, /*info=*/nullptr);
  base::Value* objects = setting.FindListKey(kObjectListKey);
  if (!objects)
    return;

  base::Value::ListView object_list = objects->GetList();
  auto it = std::find(object_list.begin(), object_list.end(), object);
  if (it != object_list.end())
    objects->EraseListIter(it);

  SetWebsiteSetting(origin, std::move(setting));
  NotifyPermissionRevoked(origin);
}

void ChooserContextBase::RevokeObjectPermission(const url::Origin& origin,
                                                const base::StringPiece key) {
  base::Value setting = GetWebsiteSetting(origin, /*info=*/nullptr);
  base::Value* objects = setting.FindListKey(kObjectListKey);
  if (!objects)
    return;

  base::Value::ListView object_list = objects->GetList();
  auto it = base::ranges::find_if(object_list, [this, &key](auto& object) {
    return IsValidObject(object) && GetKeyForObject(object) == key;
  });
  if (it != object_list.end())
    objects->EraseListIter(it);

  SetWebsiteSetting(origin, std::move(setting));
  NotifyPermissionRevoked(origin);
}

bool ChooserContextBase::HasGrantedObjects(const url::Origin& origin) {
  base::Value setting = GetWebsiteSetting(origin, /*info=*/nullptr);
  base::Value* objects = setting.FindListKey(kObjectListKey);

  return objects && !objects->GetList().empty();
}

std::string ChooserContextBase::GetKeyForObject(const base::Value& object) {
  return std::string();
}

bool ChooserContextBase::IsOffTheRecord() {
  return host_content_settings_map_->IsOffTheRecord();
}

void ChooserContextBase::NotifyPermissionChanged() {
  for (auto& observer : permission_observer_list_) {
    observer.OnChooserObjectPermissionChanged(guard_content_settings_type_,
                                              data_content_settings_type_);
  }
}

void ChooserContextBase::NotifyPermissionRevoked(const url::Origin& origin) {
  for (auto& observer : permission_observer_list_) {
    observer.OnChooserObjectPermissionChanged(guard_content_settings_type_,
                                              data_content_settings_type_);
    observer.OnPermissionRevoked(origin);
  }
}

base::Value ChooserContextBase::GetWebsiteSetting(
    const url::Origin& origin,
    content_settings::SettingInfo* info) {
  std::unique_ptr<base::Value> value =
      host_content_settings_map_->GetWebsiteSetting(
          origin.GetURL(), GURL(), data_content_settings_type_, info);
  if (value)
    return base::Value::FromUniquePtrValue(std::move(value));
  return base::Value(base::Value::Type::DICTIONARY);
}

void ChooserContextBase::SetWebsiteSetting(const url::Origin& origin,
                                           base::Value value) {
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      origin.GetURL(), GURL(), data_content_settings_type_,
      base::Value::ToUniquePtrValue(std::move(value)));
}

}  // namespace permissions
