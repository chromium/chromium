// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SAMPLE_SYSTEM_WEB_APP_UI_UNTRUSTED_SAMPLE_SYSTEM_WEB_APP_UI_H_
#define CHROMEOS_COMPONENTS_SAMPLE_SYSTEM_WEB_APP_UI_UNTRUSTED_SAMPLE_SYSTEM_WEB_APP_UI_H_

#include "ui/webui/untrusted_web_ui_controller.h"
#include "ui/webui/webui_config.h"

#if defined(OFFICIAL_BUILD)
#error Sample System Web App should only be included in unofficial builds.
#endif

namespace content {
class WebUI;
}  // namespace content

namespace chromeos {

class UntrustedSampleSystemWebAppUIConfig : public ui::WebUIConfig {
 public:
  UntrustedSampleSystemWebAppUIConfig();
  ~UntrustedSampleSystemWebAppUIConfig() override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;
};

class UntrustedSampleSystemWebAppUI : public ui::UntrustedWebUIController {
 public:
  explicit UntrustedSampleSystemWebAppUI(content::WebUI* web_ui);
  UntrustedSampleSystemWebAppUI(const UntrustedSampleSystemWebAppUI&) = delete;
  UntrustedSampleSystemWebAppUI& operator=(
      const UntrustedSampleSystemWebAppUI&) = delete;
  ~UntrustedSampleSystemWebAppUI() override;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SAMPLE_SYSTEM_WEB_APP_UI_UNTRUSTED_SAMPLE_SYSTEM_WEB_APP_UI_H_
