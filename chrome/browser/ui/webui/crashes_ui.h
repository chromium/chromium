// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CRASHES_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CRASHES_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace base {
class RefCountedMemory;
}

class CrashesUI;

class CrashesUIConfig : public content::DefaultWebUIConfig<CrashesUI> {
 public:
  CrashesUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUICrashesHost) {}
};

class CrashesUI : public content::WebUIController {
 public:
  explicit CrashesUI(content::WebUI* web_ui);

  CrashesUI(const CrashesUI&) = delete;
  CrashesUI& operator=(const CrashesUI&) = delete;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CRASHES_UI_H_
