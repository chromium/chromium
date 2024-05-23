// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_WEBUI_UI_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_WEBUI_UI_CONTROLLER_H_

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "ui/base/webui/resource_path.h"

namespace ash::kiosk_vision {

// Alias for a callback with the same signature as the webui helper function
// `webui::SetupWebUIDataSource`.
//
// DEPS rules disallow this file to include the `webui::SetupWebUIDataSource`
// helper from //chrome/browser/, so this callback is used instead.
using SetupWebUIDataSourceCallback = base::RepeatingCallback<void(
    content::WebUIDataSource* source,
    base::span<const webui::ResourcePath> resources,
    int default_resource)>;

// The WebUI controller for chrome://kiosk-vision-internals. This page displays
// development and debugging information for the Kiosk Vision feature.
class UIController : public content::WebUIController {
 public:
  UIController(content::WebUI* web_ui,
               SetupWebUIDataSourceCallback setup_callback);
  UIController(const UIController&) = delete;
  UIController& operator=(const UIController&) = delete;
  ~UIController() override;
};

// The WebUIConfig for chrome://kiosk-vision-internals.
class UIConfig : public content::WebUIConfig {
 public:
  explicit UIConfig(SetupWebUIDataSourceCallback setup_callback);
  UIConfig(const UIConfig&) = delete;
  UIConfig& operator=(const UIConfig&) = delete;
  ~UIConfig() override;

  // `content::WebUIConfig` implementation:
  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  SetupWebUIDataSourceCallback setup_callback_;
};

}  // namespace ash::kiosk_vision

#endif  // CHROMEOS_ASH_COMPONENTS_KIOSK_VISION_WEBUI_UI_CONTROLLER_H_
