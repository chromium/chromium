// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMMON_INTENT_HELPER_LINK_HANDLER_MODEL_DELEGATE_H_
#define COMPONENTS_ARC_COMMON_INTENT_HELPER_LINK_HANDLER_MODEL_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "components/arc/common/intent_helper/activity_icon_loader.h"

namespace arc {

// LinkHandlerModelDelegate provides API to access to ARC Activity Icons cache.
class LinkHandlerModelDelegate {
 public:
  virtual ~LinkHandlerModelDelegate() = default;

  // internal::ActivityIconLoader types.
  using ActivityIconLoader = internal::ActivityIconLoader;
  using ActivityName = internal::ActivityIconLoader::ActivityName;
  using ActivityToIconsMap = internal::ActivityIconLoader::ActivityToIconsMap;
  using GetResult = internal::ActivityIconLoader::GetResult;
  using OnIconsReadyCallback =
      internal::ActivityIconLoader::OnIconsReadyCallback;

  struct IntentHandlerInfo {
    IntentHandlerInfo(const std::string& name,
                      const std::string& package_name,
                      const std::string& activity_name)
        : name(name),
          package_name(package_name),
          activity_name(activity_name) {}
    ~IntentHandlerInfo() = default;

    // The name of the package used as a description text.
    std::string name;
    // The name of the package used as an ID.
    std::string package_name;
    // A hint for retrieving the package's icon.
    std::string activity_name;
  };

  using RequestUrlHandlerListCallback =
      base::OnceCallback<void(std::vector<IntentHandlerInfo>)>;

  // Retrieves icons for the |activities| and calls |callback|.
  // See internal::ActivityIconLoader::GetActivityIcons() for more details.
  virtual GetResult GetActivityIcons(
      const std::vector<ActivityName>& activities,
      OnIconsReadyCallback callback) = 0;

  // Calls RequestUrlHandlerList mojo API.
  virtual bool RequestUrlHandlerList(
      const std::string& url,
      RequestUrlHandlerListCallback callback) = 0;

  // Calls HandleUrl mojo API.
  virtual bool HandleUrl(const std::string& url,
                         const std::string& package_name) = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMMON_INTENT_HELPER_LINK_HANDLER_MODEL_DELEGATE_H_
