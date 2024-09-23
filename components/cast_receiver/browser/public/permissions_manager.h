// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_PERMISSIONS_MANAGER_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_PERMISSIONS_MANAGER_H_

#include <string>
#include <vector>

#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace cast_receiver {

class PermissionsManager {
 public:
  // Returns the PermissionsManager associated with |web_contents| if such an
  // instance exists.
  static PermissionsManager* GetInstance(content::WebContents& web_contents);

  virtual ~PermissionsManager() = default;

  // Returns the Application ID associated with these permissions.
  virtual const std::string& GetAppId() const = 0;

  // Determines whether a |permission| should be extended to this application at
  // |url|.
  //
  // TODO(crbug.com/1383300): Use a url::Origin instead of GURL for improved
  // security when permissions checking.
  virtual blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission,
      const GURL& url) const = 0;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_PERMISSIONS_MANAGER_H_
