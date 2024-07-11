// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permissions_reprompt_controller_android.h"

#include "base/check.h"
#include "base/not_fatal_until.h"
#include "components/permissions/android/android_permission_util.h"
#include "components/permissions/permissions_client.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace permissions {

void PermissionsRepromptControllerAndroid::RepromptPermissionRequest(
    const std::vector<ContentSettingsType>& content_settings_types,
    ContentSettingsType permission_context_content_setting_type,
    RepromptPermissionRequestCallback callback) {
  DCHECK_EQ(permissions::ShouldRepromptUserForPermissions(
                &GetWebContents(), content_settings_types),
            permissions::PermissionRepromptState::kShow)
      << "Caller should check ShouldRepromptUserForPermissions before "
         "reprompting";

  const auto filtered_content_settings_types =
      GetContentSettingsWithMissingRequiredAndroidPermissions(
          content_settings_types, &GetWebContents());
  RepromptPermissionRequestInternal(
      content_settings_types, filtered_content_settings_types,
      permission_context_content_setting_type, std::move(callback));
}

void PermissionsRepromptControllerAndroid::OnRepromptPermissionRequestDone(
    const RequestKey& request_key,
    bool success) {
  auto it = pending_callbacks_.find(request_key);
  CHECK(it != pending_callbacks_.end(), base::NotFatalUntil::M130);

  for (auto& callback : it->second.second) {
    std::move(callback).Run(success);
  }
  pending_callbacks_.erase(request_key);
}

void PermissionsRepromptControllerAndroid::RepromptPermissionRequestInternal(
    const std::vector<ContentSettingsType>& content_settings_types,
    const std::vector<ContentSettingsType>& filtered_content_settings_types,
    ContentSettingsType permission_context_content_setting_type,
    RepromptPermissionRequestCallback callback) {
  RequestKey key(filtered_content_settings_types);
  auto it = pending_callbacks_.find(key);
  if (it != pending_callbacks_.end()) {
    auto& context_set = it->second.first;
    // Simply bail out if we are requesting duplicated requests from the
    // same PermissionContext
    if (context_set.find(permission_context_content_setting_type) !=
        context_set.end())
      return;

    context_set.insert(permission_context_content_setting_type);
    auto& callbacks = it->second.second;
    callbacks.push_back(std::move(callback));
    return;
  }

  PermissionsContextSet new_context_set{
      permission_context_content_setting_type};
  RequestCallbacksList new_callbacks;
  new_callbacks.push_back(std::move(callback));
  pending_callbacks_[key] =
      std::make_pair(std::move(new_context_set), std::move(new_callbacks));
  PermissionsClient::Get()->RepromptForAndroidPermissions(
      &GetWebContents(), content_settings_types,
      filtered_content_settings_types, key.required_permissions,
      key.optional_permissions,
      base::BindOnce(&PermissionsRepromptControllerAndroid::
                         OnRepromptPermissionRequestDone,
                     weak_factory_.GetWeakPtr(), key));
}

PermissionsRepromptControllerAndroid::PermissionsRepromptControllerAndroid(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PermissionsRepromptControllerAndroid>(
          *web_contents) {}

PermissionsRepromptControllerAndroid::~PermissionsRepromptControllerAndroid() {
  for (auto& it : pending_callbacks_) {
    for (auto& callback : it.second.second) {
      // If we don't know the state of the OS permission due to destroyed
      // web_content, assume we don't have the permission.
      std::move(callback).Run(/* permission_granted */ false);
    }
  }

  pending_callbacks_.clear();
}

PermissionsRepromptControllerAndroid::RequestKey::RequestKey(
    const std::vector<ContentSettingsType>& content_setting_types) {
  AppendRequiredAndOptionalAndroidPermissionsForContentSettings(
      content_setting_types, required_permissions, optional_permissions);
}

PermissionsRepromptControllerAndroid::RequestKey::RequestKey(
    const RequestKey& RequestKey) = default;
PermissionsRepromptControllerAndroid::RequestKey::RequestKey(
    RequestKey&& RequestKey) = default;

PermissionsRepromptControllerAndroid::RequestKey::~RequestKey() = default;

bool PermissionsRepromptControllerAndroid::RequestKey::operator<(
    const RequestKey& rhs) const {
  return std::tie(required_permissions, optional_permissions) <
         std::tie(rhs.required_permissions, rhs.optional_permissions);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PermissionsRepromptControllerAndroid);

}  // namespace permissions
