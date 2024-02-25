// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMMON_INTENT_HELPER_ARC_INTENT_HELPER_MOJO_DELEGATE_H_
#define COMPONENTS_ARC_COMMON_INTENT_HELPER_ARC_INTENT_HELPER_MOJO_DELEGATE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "components/arc/common/intent_helper/activity_icon_loader.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/image/image_skia.h"

namespace arc {

// This class provides API to use mojo connection.
// For ash-chrome, it connects to ARC. For lacros-chrome, it connects to
// ash-chrome which forwards to ARC.
class ArcIntentHelperMojoDelegate {
 public:
  virtual ~ArcIntentHelperMojoDelegate() = default;

  // To make ActivityName type consistent between ArcIconCacheDelegate, use
  // internal::ActivityIconLoader::ActivityName.
  using ActivityName = internal::ActivityIconLoader::ActivityName;

  // Following structs basically refer to //ash/components/arc/mojom.
  // Convert arc::mojom and crosapi::mojom into common structs available from
  // both ash and lacros.
  // Some unnecessary parameters are dropped here.

  // Describes an intent.
  // See //ash/components/arc/mojom/intent_helper.mojom for more details.
  struct IntentInfo {
    IntentInfo(std::string action,
               std::optional<std::vector<std::string>> categories,
               std::optional<std::string> data,
               std::optional<std::string> type,
               bool ui_bypassed,
               std::optional<base::flat_map<std::string, std::string>> extras);
    IntentInfo(const IntentInfo& other);
    IntentInfo& operator=(const IntentInfo&) = delete;
    ~IntentInfo();

    std::string action;
    std::optional<std::vector<std::string>> categories;
    std::optional<std::string> data;
    std::optional<std::string> type;
    bool ui_bypassed;
    std::optional<base::flat_map<std::string, std::string>> extras;
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

  // Describes a package that can handle an intent.
  // See //ash/components/arc/mojom/intent_helper.mojom for more details.
  struct IntentHandlerInfo {
    IntentHandlerInfo(std::string name,
                      std::string package_name,
                      std::string activity_name,
                      bool is_preferred,
                      std::optional<std::string> fallback_url);
    IntentHandlerInfo(const IntentHandlerInfo& other);
    IntentHandlerInfo& operator=(const IntentHandlerInfo&) = default;
    ~IntentHandlerInfo();

    // The name of the package used as a description text.
    std::string name;
    // The name of the package used as an ID.
    std::string package_name;
    // A hint for retrieving the package's icon.
    std::string activity_name;
    // Set to true if the package is set as a preferred package.
    bool is_preferred;
    // RequestUrlHandlerList may fill |fallback_url| when it is called with an
    // intent: URL.
    std::optional<std::string> fallback_url;
  };

  using RequestUrlHandlerListCallback =
      base::OnceCallback<void(std::vector<IntentHandlerInfo>)>;

  using RequestTextSelectionActionsCallback =
      base::OnceCallback<void(std::vector<TextSelectionAction>)>;

  // Returns true if ARC is available.
  virtual bool IsArcAvailable() = 0;

  // Returns true if RequestUrlHandlerList is available.
  virtual bool IsRequestUrlHandlerListAvailable() = 0;

  // Returns true if RequestTextSelectionActions is available.
  virtual bool IsRequestTextSelectionActionsAvailable() = 0;

  // Calls RequestUrlHandlerList mojo API.
  virtual bool RequestUrlHandlerList(
      const std::string& url,
      RequestUrlHandlerListCallback callback) = 0;

  // Calls RequestTextSelectionActions mojo API.
  virtual bool RequestTextSelectionActions(
      const std::string& text,
      ui::ResourceScaleFactor scale_factor,
      RequestTextSelectionActionsCallback callback) = 0;

  // Calls HandleUrl mojo API.
  virtual bool HandleUrl(const std::string& url,
                         const std::string& package_name) = 0;

  // Calls HandleIntent mojo API.
  virtual bool HandleIntent(const IntentInfo& intent,
                            const ActivityName& activity) = 0;

  // Calls AddPreferredPackage mojo API.
  virtual bool AddPreferredPackage(const std::string& package_name) = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMMON_INTENT_HELPER_ARC_INTENT_HELPER_MOJO_DELEGATE_H_
