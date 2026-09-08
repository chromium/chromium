// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ARC_POWER_CONTROL_ARC_POWER_CONTROL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ARC_POWER_CONTROL_ARC_POWER_CONTROL_UI_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class ApplicationLocaleStorage;
class GURL;

namespace content {
class BrowserContext;
class WebUI;
}  // namespace content

namespace ash {

class ArcPowerControlUI;

// WebUIConfig for chrome://arc-power-control
class ArcPowerControlUIConfig : public content::WebUIConfig {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  explicit ArcPowerControlUIConfig(
      const ApplicationLocaleStorage* application_locale_storage);

  ArcPowerControlUIConfig(const ArcPowerControlUIConfig&) = delete;
  ArcPowerControlUIConfig& operator=(const ArcPowerControlUIConfig&) = delete;

  ~ArcPowerControlUIConfig() override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui,
      const GURL& url) override;

 private:
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
};

// WebUI controller for ARC power control.
class ArcPowerControlUI : public content::WebUIController {
 public:
  ArcPowerControlUI(content::WebUI* web_ui,
                    const std::string& application_locale);

  ArcPowerControlUI(const ArcPowerControlUI&) = delete;
  ArcPowerControlUI& operator=(const ArcPowerControlUI&) = delete;

  ~ArcPowerControlUI() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ARC_POWER_CONTROL_ARC_POWER_CONTROL_UI_H_
