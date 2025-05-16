// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_RESOLVERS_PERMISSION_RESOLVER_H_
#define COMPONENTS_PERMISSIONS_RESOLVERS_PERMISSION_RESOLVER_H_

#include <optional>

#include "base/values.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/request_type.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom.h"

namespace permissions {

// Interface that allows implementing a permission resolver. Subclasses should
// implement logic for one or more permission types. Each object of subclasses
// should capture all information of a particular permission request information
// (`PermissionDescriptorPtr`) and implement the interface methods in order to
// mediate between the stored permission state and the request.
class PermissionResolver {
 public:
  // `PromptParameters` are returned when the UI queries the resolver to
  // determine what to prompt the user for.
  struct PromptParameters {
    PromptParameters();
    ~PromptParameters();
    base::Value missing_options;
    std::u16string prompt_text;
    std::vector<std::u16string> radio_button_labels;
    int preselected_radio_button_index = -1;  // -1 represents no preselection
  };

  virtual ~PermissionResolver() = default;

  // Determines the permission status of the request given the user's permission
  // state.
  virtual blink::mojom::PermissionStatus DeterminePermissionStatus(
      const base::Value& value) const = 0;

  // Determines the user's new permission state given a user decision for the
  // request.
  virtual base::Value ComputePermissionDecisionResult(
      const base::Value& previous_value,
      ContentSetting decision,
      std::optional<base::Value> prompt_options) const = 0;

  // Determines the `PromptParameters` for the current request given the
  // `current_setting_state` which is the fully coalesced current settings
  // value.
  // Can be queried by the UI through the `PermissionRequest` to determine what
  // to prompt the user for. `PermissionRequest` objects hold a
  // PermissionRequestData instance, which holds the PermissionResolver for the
  // particular request.
  virtual PromptParameters GetPromptParameters(
      const base::Value& current_setting_state) const = 0;

  // Utility method to obtain the `ContentSettingsType` of the object if it
  // exists.
  std::optional<ContentSettingsType> GetContentSettingsType() const {
    return content_settings_type_;
  }

  // Utility method to obtain the `RequestType` of the object if it exists.
  std::optional<RequestType> GetRequestType() const { return request_type_; }

 protected:
  explicit PermissionResolver(ContentSettingsType content_settings_type);
  explicit PermissionResolver(RequestType request_type);

 private:
  std::optional<ContentSettingsType> content_settings_type_;
  std::optional<RequestType> request_type_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_RESOLVERS_PERMISSION_RESOLVER_H_
