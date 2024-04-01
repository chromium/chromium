// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMPONENTS_COMPONENTS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_COMPONENTS_COMPONENTS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace base {
class RefCountedMemory;
}

class ComponentsUI;

// WebUIConfig for chrome://components
class ComponentsUIConfig : public content::DefaultWebUIConfig<ComponentsUI> {
 public:
  ComponentsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIComponentsHost) {}
};

class ComponentsUI : public content::WebUIController {
 public:
  explicit ComponentsUI(content::WebUI* web_ui);

  ComponentsUI(const ComponentsUI&) = delete;
  ComponentsUI& operator=(const ComponentsUI&) = delete;

  ~ComponentsUI() override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);
};

#endif  // CHROME_BROWSER_UI_WEBUI_COMPONENTS_COMPONENTS_UI_H_
