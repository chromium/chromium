// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_APPLICATION_CONFIG_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_APPLICATION_CONFIG_H_

#include <optional>
#include <string>
#include <vector>

#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cast_receiver {

// This class provides data about an application to be used to configure its
// launch.
struct ApplicationConfig {
  struct ContentPermissions {
    ContentPermissions();
    ContentPermissions(std::vector<blink::PermissionType> permissions_set,
                       std::vector<url::Origin> origins);
    ~ContentPermissions();

    ContentPermissions(const ContentPermissions& config);
    ContentPermissions(ContentPermissions&& config);
    ContentPermissions& operator=(const ContentPermissions& config);
    ContentPermissions& operator=(ContentPermissions&& config);

    // Permissions that should be allowed for the associated application.
    //
    // TODO(crbug.com/1383326): Ensure this each permission is valid per an
    // allow list maintained by this component.
    std::vector<blink::PermissionType> permissions;

    // Origins that should be allowed the above permissions in addition to that
    // of the root url associated with this app. All origins must be valid
    // (i.e. non-opaque).
    std::vector<url::Origin> additional_origins;
  };

  ApplicationConfig();
  ApplicationConfig(std::string id,
                    std::string name,
                    ContentPermissions content_permissions);
  ~ApplicationConfig();

  ApplicationConfig(const ApplicationConfig& config);
  ApplicationConfig(ApplicationConfig&& config);
  ApplicationConfig& operator=(const ApplicationConfig& config);
  ApplicationConfig& operator=(ApplicationConfig&& config);

  // The Application ID.
  std::string app_id;

  // The name that should be displayed.
  std::string display_name;

  // The URL for this application, if any.
  std::optional<GURL> url;

  // Permissions to be granted to this application.
  ContentPermissions permissions;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_APPLICATION_CONFIG_H_
