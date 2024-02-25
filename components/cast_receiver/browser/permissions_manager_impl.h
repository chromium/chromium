// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PERMISSIONS_MANAGER_IMPL_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PERMISSIONS_MANAGER_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/supports_user_data.h"
#include "components/cast_receiver/browser/public/permissions_manager.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class WebContents;
}  // namespace content

namespace cast_receiver {

class PermissionsManagerImpl : public PermissionsManager,
                               public base::SupportsUserData::Data {
 public:
  static PermissionsManagerImpl* CreateInstance(
      content::WebContents& web_contents,
      std::string app_id);

  explicit PermissionsManagerImpl(std::string app_id);
  ~PermissionsManagerImpl() override;

  // Adds a new permission for |app_url_| and all additional origins added to
  // this instance.
  void AddPermission(blink::PermissionType permission);

  // Adds a new origin to which all permissions added to this instance should be
  // extended.
  void AddOrigin(url::Origin origin);

  // PermissionsManager implementation.
  const std::string& GetAppId() const override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission,
      const GURL& url) const override;

 private:
  // Application ID for the associated app.
  const std::string app_id_;

  // URL for this application, if any.
  const std::optional<GURL> app_url_;

  // Permissions to extend to this app.
  std::vector<blink::PermissionType> permissions_;

  // Additional origins for this app.
  std::vector<url::Origin> additional_origins_;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PERMISSIONS_MANAGER_IMPL_H_
