// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_RESOLVERS_PERMISSION_RESOLVER_H_
#define COMPONENTS_PERMISSIONS_RESOLVERS_PERMISSION_RESOLVER_H_

#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"

namespace permissions {

// Interface that allows implementing a permission resolver. Subclasses should
// implement logic for one or more permission types. Each object of subclasses
// should capture all information of a particular permission request information
// (`PermissionDescriptorPtr`) and implement the interface methods in order to
// mediate between the stored permission state and the request.
class PermissionResolver {
 public:
  struct PermissionSetting {
    explicit PermissionSetting(ContentSetting permission_content_setting,
                               base::Value permission_options = base::Value());
    PermissionSetting(PermissionSetting& other);
    bool operator==(const PermissionSetting& other) const;

    ContentSetting content_setting;
    base::Value options;
  };

  virtual ~PermissionResolver() = default;

  // Determines the permission status of the request given the user's permission
  // state.
  virtual blink::mojom::PermissionStatus DeterminePermissionStatus(
      PermissionSetting setting) = 0;

  // Determines the user's new permission state given a user decision for the
  // request.
  virtual PermissionSetting ComputePermissionDecisionResult(
      PermissionSetting previous_setting,
      ContentSetting decision,
      std::optional<base::Value> prompt_options) = 0;

  // Utility method to obtain the `ContentSettingsType` of the object.
  ContentSettingsType GetContentSettingsType() const {
    return content_settings_type_;
  }

 protected:
  explicit PermissionResolver(ContentSettingsType content_settings_type);

 private:
  ContentSettingsType content_settings_type_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_RESOLVERS_PERMISSION_RESOLVER_H_
