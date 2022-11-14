// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/object_permission_context_base.h"

#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "url/origin.h"

namespace permissions {

const char kObjectListKey[] = "chosen-objects";

ObjectPermissionContextBase::ObjectPermissionContextBase(
    ContentSettingsType guard_content_settings_type,
    ContentSettingsType data_content_settings_type,
    HostContentSettingsMap* host_content_settings_map)
    : guard_content_settings_type_(guard_content_settings_type),
      data_content_settings_type_(data_content_settings_type),
      host_content_settings_map_(host_content_settings_map) {
  DCHECK(host_content_settings_map_);
}

ObjectPermissionContextBase::ObjectPermissionContextBase(
    ContentSettingsType data_content_settings_type,
    HostContentSettingsMap* host_content_settings_map)
    : guard_content_settings_type_(absl::nullopt),
      data_content_settings_type_(data_content_settings_type),
      host_content_settings_map_(host_content_settings_map) {
  DCHECK(host_content_settings_map_);
}

ObjectPermissionContextBase::~ObjectPermissionContextBase() {
  FlushScheduledSaveSettingsCalls();
}

ObjectPermissionContextBase::Object::Object(
    const url::Origin& origin,
    base::Value value,
    content_settings::SettingSource source,
    bool incognito)
    : origin(origin.GetURL()),
      value(std::move(value)),
      source(source),
      incognito(incognito) {}

ObjectPermissionContextBase::Object::~Object() = default;

std::unique_ptr<ObjectPermissionContextBase::Object>
ObjectPermissionContextBase::Object::Clone() {
  return std::make_unique<Object>(url::Origin::Create(origin), value.Clone(),
                                  source, incognito);
}

void ObjectPermissionContextBase::PermissionObserver::OnObjectPermissionChanged(
    absl::optional<ContentSettingsType> guard_content_settings_type,
    ContentSettingsType data_content_settings_type) {}

void ObjectPermissionContextBase::PermissionObserver::OnPermissionRevoked(
    const url::Origin& origin) {}

void ObjectPermissionContextBase::AddObserver(PermissionObserver* observer) {
  permission_observer_list_.AddObserver(observer);
}

void ObjectPermissionContextBase::RemoveObserver(PermissionObserver* observer) {
  permission_observer_list_.RemoveObserver(observer);
}

bool ObjectPermissionContextBase::CanRequestObjectPermission(
    const url::Origin& origin) {
  if (!guard_content_settings_type_)
    return true;

  ContentSetting content_setting =
      host_content_settings_map_->GetContentSetting(
          origin.GetURL(), GURL(), *guard_content_settings_type_);
  DCHECK(content_setting == CONTENT_SETTING_ASK ||
         content_setting == CONTENT_SETTING_BLOCK);
  return content_setting == CONTENT_SETTING_ASK;
}

std::unique_ptr<ObjectPermissionContextBase::Object>
ObjectPermissionContextBase::GetGrantedObject(const url::Origin& origin,
                                              const base::StringPiece key) {
  if (!CanRequestObjectPermission(origin))
    return nullptr;

  const auto& origin_objects_it = objects().find(origin);
  if (origin_objects_it == objects().end())
    return nullptr;

  const auto& object_it = origin_objects_it->second.find(std::string(key));

  if (object_it == origin_objects_it->second.end())
    return nullptr;

  return object_it->second->Clone();
}

std::vector<std::unique_ptr<ObjectPermissionContextBase::Object>>
ObjectPermissionContextBase::GetGrantedObjects(const url::Origin& origin) {
  if (!CanRequestObjectPermission(origin))
    return {};

  const auto& origin_objects_it = objects().find(origin);
  if (origin_objects_it == objects().end())
    return {};

  std::vector<std::unique_ptr<Object>> results;
  for (const auto& object : origin_objects_it->second)
    results.push_back(object.second->Clone());

  return results;
}

std::vector<std::unique_ptr<ObjectPermissionContextBase::Object>>
ObjectPermissionContextBase::GetAllGrantedObjects() {
  std::vector<std::unique_ptr<Object>> results;
  for (const auto& objects_entry : objects()) {
    if (!CanRequestObjectPermission(objects_entry.first))
      continue;

    for (const auto& object : objects_entry.second)
      results.push_back(object.second->Clone());
  }
  return results;
}

void ObjectPermissionContextBase::GrantObjectPermission(
    const url::Origin& origin,
    base::Value object) {
  DCHECK(IsValidObject(object));

  const std::string key = GetKeyForObject(object);

  objects()[origin][key] = std::make_unique<Object>(
      origin, std::move(object),
      content_settings::SettingSource::SETTING_SOURCE_USER,
      host_content_settings_map_->IsOffTheRecord());

  ScheduleSaveWebsiteSetting(origin);
  NotifyPermissionChanged();
}

void ObjectPermissionContextBase::UpdateObjectPermission(
    const url::Origin& origin,
    const base::Value& old_object,
    base::Value new_object) {
  auto origin_objects_it = objects().find(origin);
  if (origin_objects_it == objects().end())
    return;

  std::string key = GetKeyForObject(old_object);
  auto object_it = origin_objects_it->second.find(key);
  if (object_it == origin_objects_it->second.end())
    return;

  origin_objects_it->second.erase(object_it);
  key = GetKeyForObject(new_object);
  DCHECK(!base::Contains(origin_objects_it->second, key));

  GrantObjectPermission(origin, std::move(new_object));
}

void ObjectPermissionContextBase::RevokeObjectPermission(
    const url::Origin& origin,
    const base::Value& object) {
  DCHECK(IsValidObject(object));

  RevokeObjectPermission(origin, GetKeyForObject(object));
}

void ObjectPermissionContextBase::RevokeObjectPermission(
    const url::Origin& origin,
    const base::StringPiece key) {
  auto origin_objects_it = objects().find(origin);
  if (origin_objects_it == objects().end())
    return;

  auto object_it = origin_objects_it->second.find(std::string(key));
  if (object_it == origin_objects_it->second.end())
    return;

  origin_objects_it->second.erase(object_it);

  if (!origin_objects_it->second.size())
    objects().erase(origin_objects_it);

  ScheduleSaveWebsiteSetting(origin);
  NotifyPermissionRevoked(origin);
}

bool ObjectPermissionContextBase::HasGrantedObjects(const url::Origin& origin) {
  auto origin_objects_it = objects().find(origin);
  return origin_objects_it != objects().end() &&
         !origin_objects_it->second.empty();
}

void ObjectPermissionContextBase::FlushScheduledSaveSettingsCalls() {
  // Persist any pending object updates that did not have the chance to be
  // persisted yet.
  while (!origins_with_scheduled_save_settings_calls_.empty()) {
    const url::Origin origin =
        *(origins_with_scheduled_save_settings_calls_.begin());
    SaveWebsiteSetting(origin);
  }
}

bool ObjectPermissionContextBase::IsOffTheRecord() {
  return host_content_settings_map_->IsOffTheRecord();
}

void ObjectPermissionContextBase::NotifyPermissionChanged() {
  for (auto& observer : permission_observer_list_) {
    observer.OnObjectPermissionChanged(guard_content_settings_type_,
                                       data_content_settings_type_);
  }
}

void ObjectPermissionContextBase::NotifyPermissionRevoked(
    const url::Origin& origin) {
  for (auto& observer : permission_observer_list_) {
    observer.OnObjectPermissionChanged(guard_content_settings_type_,
                                       data_content_settings_type_);
    observer.OnPermissionRevoked(origin);
  }
}

base::Value ObjectPermissionContextBase::GetWebsiteSetting(
    const url::Origin& origin,
    content_settings::SettingInfo* info) {
  base::Value value = host_content_settings_map_->GetWebsiteSetting(
      origin.GetURL(), GURL(), data_content_settings_type_, info);
  if (value.is_none())
    return base::Value(base::Value::Type::DICTIONARY);
  return value;
}

void ObjectPermissionContextBase::SaveWebsiteSetting(
    const url::Origin& origin) {
  auto scheduled_save_it =
      origins_with_scheduled_save_settings_calls_.find(origin);
  if (scheduled_save_it == origins_with_scheduled_save_settings_calls_.end()) {
    // Another scheduled `SaveWebsiteSetting` call has handled this origin
    // already.
    return;
  }
  origins_with_scheduled_save_settings_calls_.erase(scheduled_save_it);

  auto origin_objects_it = objects().find(origin);

  if (origin_objects_it == objects().end()) {
    host_content_settings_map_->SetWebsiteSettingDefaultScope(
        origin.GetURL(), GURL(), data_content_settings_type_, base::Value());
    return;
  }

  base::Value objects_list(base::Value::Type::LIST);
  for (const auto& object : origin_objects_it->second) {
    objects_list.Append(object.second->value.Clone());
  }
  base::Value website_setting_value(base::Value::Type::DICTIONARY);
  website_setting_value.SetKey(kObjectListKey, std::move(objects_list));
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      origin.GetURL(), GURL(), data_content_settings_type_,
      std::move(website_setting_value));
}

void ObjectPermissionContextBase::ScheduleSaveWebsiteSetting(
    const url::Origin& origin) {
  bool success =
      origins_with_scheduled_save_settings_calls_.insert(origin).second;
  if (!success) {
    // There is already a scheduled `SaveWebsiteSetting` call, no need to
    // schedule another.
    return;
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ObjectPermissionContextBase::SaveWebsiteSetting,
                     weak_factory_.GetWeakPtr(), origin));
}

std::vector<std::unique_ptr<ObjectPermissionContextBase::Object>>
ObjectPermissionContextBase::GetWebsiteSettingObjects() {
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

void ObjectPermissionContextBase::LoadWebsiteSettingsIntoObjects() {
  auto loaded_objects = GetWebsiteSettingObjects();
  for (auto& object : loaded_objects) {
    objects_[url::Origin::Create(object->origin)].emplace(
        GetKeyForObject(object->value), std::move(object));
  }
}

ObjectPermissionContextBase::ObjectMap& ObjectPermissionContextBase::objects() {
  if (!objects_initialized_) {
    LoadWebsiteSettingsIntoObjects();
    objects_initialized_ = true;
  }
  return objects_;
}

}  // namespace permissions
