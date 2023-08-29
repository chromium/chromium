// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SCALABLE_IPH_SCALABLE_IPH_DEBUG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SCALABLE_IPH_SCALABLE_IPH_DEBUG_UI_H_

#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace ash {

class ScalableIphDebugUI;

class ScalableIphDebugUIConfig
    : public content::DefaultWebUIConfig<ScalableIphDebugUI> {
 public:
  ScalableIphDebugUIConfig()
      : content::DefaultWebUIConfig<ScalableIphDebugUI>(
            content::kChromeUIUntrustedScheme,
            scalable_iph::kScalableIphDebugHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class ScalableIphDebugUI : public ui::UntrustedWebUIController {
 public:
  explicit ScalableIphDebugUI(content::WebUI* web_ui);
  ~ScalableIphDebugUI() override;

 private:
  void HandleRequest(const std::string& path,
                     content::WebUIDataSource::GotDataCallback callback);

  base::WeakPtrFactory<ScalableIphDebugUI> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SCALABLE_IPH_SCALABLE_IPH_DEBUG_UI_H_
