// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSIONS_REPROMPT_CONTROLLER_ANDROID_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSIONS_REPROMPT_CONTROLLER_ANDROID_H_

#include <algorithm>
#include <map>
#include <tuple>
#include <vector>

#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace permissions {

// A controller to be used for reprompting Android permission requests, which
// have been previously accepted/denied. This class exists to hold a map of
// pending callback instances to filter out duplicating requests from the same
// WebContent.
class PermissionsRepromptControllerAndroid
    : public content::WebContentsUserData<
          PermissionsRepromptControllerAndroid> {
 public:
  PermissionsRepromptControllerAndroid(
      const PermissionsRepromptControllerAndroid&) = delete;
  PermissionsRepromptControllerAndroid& operator=(
      const PermissionsRepromptControllerAndroid&) = delete;
  ~PermissionsRepromptControllerAndroid() override;

  using RepromptPermissionRequestCallback = base::OnceCallback<void(bool)>;

  // Reprompts permission request of the given list |content_settings_types|.
  // |callback| will be run synchonously if the request is not existing on the
  // |pending_callbacks_|, otherwise, it will be deferred.
  void RepromptPermissionRequest(
      const std::vector<ContentSettingsType>& content_settings_types,
      ContentSettingsType permission_context_content_setting_type,
      RepromptPermissionRequestCallback callback);

 private:
  friend class content::WebContentsUserData<
      PermissionsRepromptControllerAndroid>;
  friend class PermissionsRepromptControllerAndroidTest;

  // Key to access entries in the pending callbacks map.
  struct RequestKey {
    explicit RequestKey(const std::vector<ContentSettingsType>& types);
    ~RequestKey();

    RequestKey(const RequestKey& key);
    RequestKey(RequestKey&& key);

    bool operator<(const RequestKey& rhs) const;

    std::vector<std::string> required_permissions;
    std::vector<std::string> optional_permissions;
  };

  explicit PermissionsRepromptControllerAndroid(content::WebContents* contents);

  void OnRepromptPermissionRequestDone(const RequestKey& request_key,
                                       bool success);

  void RepromptPermissionRequestInternal(
      const std::vector<ContentSettingsType>& content_settings_types,
      const std::vector<ContentSettingsType>& filtered_content_settings_types,
      ContentSettingsType permission_context_content_setting_type,
      RepromptPermissionRequestCallback callback);

  using PermissionsContextSet = std::set<ContentSettingsType>;
  using RequestCallbacksList = std::vector<RepromptPermissionRequestCallback>;
  std::map<RequestKey, std::pair<PermissionsContextSet, RequestCallbacksList>>
      pending_callbacks_;

  base::WeakPtrFactory<PermissionsRepromptControllerAndroid> weak_factory_{
      this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSIONS_REPROMPT_CONTROLLER_ANDROID_H_
