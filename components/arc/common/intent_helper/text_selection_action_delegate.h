// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMMON_INTENT_HELPER_TEXT_SELECTION_ACTION_DELEGATE_H_
#define COMPONENTS_ARC_COMMON_INTENT_HELPER_TEXT_SELECTION_ACTION_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_skia.h"

namespace arc {

// The delegate to handle RequestTextSelectionActions mojo API and required
// structs for it.
class TextSelectionActionDelegate {
 public:
  virtual ~TextSelectionActionDelegate() = default;

  // Following structs basically refer to //ash/components/arc/mojom.
  // Convert arc::mojom and crosapi::mojom into common structs available from
  // both ash and lacros.
  // Some unnecessary parameters are dropped here.

  // Describes an activity.
  // See //ash/components/arc/mojom/intent_common.mojom for more details.
  struct ActivityName {
    ActivityName(std::string package_name,
                 absl::optional<std::string> activity_name);
    ActivityName(const ActivityName& other);
    ActivityName& operator=(const ActivityName&) = delete;
    ~ActivityName();

    std::string package_name;
    absl::optional<std::string> activity_name;
  };

  // Describes an intent.
  // See //ash/components/arc/mojom/intent_helper.mojom for more details.
  struct IntentInfo {
    IntentInfo(std::string action,
               absl::optional<std::vector<std::string>> categories,
               absl::optional<std::string> data,
               absl::optional<std::string> type,
               bool ui_bypassed,
               absl::optional<base::flat_map<std::string, std::string>> extras);
    IntentInfo(const IntentInfo& other);
    IntentInfo& operator=(const IntentInfo&) = delete;
    ~IntentInfo();

    std::string action;
    absl::optional<std::vector<std::string>> categories;
    absl::optional<std::string> data;
    absl::optional<std::string> type;
    bool ui_bypassed;
    absl::optional<base::flat_map<std::string, std::string>> extras;
  };

  // Describes an action given by the android text selection delegate (e.g. open
  // maps).
  // See //ash/components/arc/mojom/intent_helper.mojom for more details.
  struct TextSelectionAction {
    TextSelectionAction(std::string app_id,
                        gfx::ImageSkia icon,
                        ActivityName activity,
                        std::string title,
                        IntentInfo action_intent);
    TextSelectionAction(const TextSelectionAction& other);
    TextSelectionAction& operator=(const TextSelectionAction&) = delete;
    ~TextSelectionAction();

    // App ID of the package.
    // Note that this parameter is not set in arc::mojom::TextSelectionAction,
    // but required in this struct.
    std::string app_id;

    // ImageSkia icon of the package.
    // Note that this parameter is not set in arc::mojom::TextSelectionAction,
    // but required in this struct.
    gfx::ImageSkia icon;

    ActivityName activity;
    std::string title;
    IntentInfo action_intent;
  };

  using RequestTextSelectionActionsCallback =
      base::OnceCallback<void(std::vector<TextSelectionAction>)>;

  // Calls RequestTextSelectionActions mojo API.
  virtual bool RequestTextSelectionActions(
      const std::string& text,
      ui::ResourceScaleFactor scale_factor,
      RequestTextSelectionActionsCallback callback) = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMMON_INTENT_HELPER_TEXT_SELECTION_ACTION_DELEGATE_H_
