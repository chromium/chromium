// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PERSONALIZATION_APP_UNTRUSTED_PERSONALIZATION_APP_UI_CONFIG_H_
#define CHROMEOS_COMPONENTS_PERSONALIZATION_APP_UNTRUSTED_PERSONALIZATION_APP_UI_CONFIG_H_

#include "ui/webui/untrusted_web_ui_controller.h"
#include "ui/webui/webui_config.h"

namespace content {
class WebUI;
}  // namespace content

namespace chromeos {

class UntrustedPersonalizationAppUIConfig : public ui::WebUIConfig {
 public:
  UntrustedPersonalizationAppUIConfig();
  ~UntrustedPersonalizationAppUIConfig() override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PERSONALIZATION_APP_UNTRUSTED_PERSONALIZATION_APP_UI_CONFIG_H_
