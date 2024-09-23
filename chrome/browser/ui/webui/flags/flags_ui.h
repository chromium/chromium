// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FLAGS_FLAGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FLAGS_FLAGS_UI_H_

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace base {
class RefCountedMemory;
}

namespace content {
class WebUIDataSource;
}

class FlagsUI;

class FlagsUIConfig : public content::DefaultWebUIConfig<FlagsUI> {
 public:
  FlagsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIFlagsHost) {}
};

class FlagsUI : public content::WebUIController {
 public:
  explicit FlagsUI(content::WebUI* web_ui);

  FlagsUI(const FlagsUI&) = delete;
  FlagsUI& operator=(const FlagsUI&) = delete;

  ~FlagsUI() override;

  static void AddStrings(content::WebUIDataSource* source);
  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

 private:
  base::WeakPtrFactory<FlagsUI> weak_factory_{this};
};
#endif  // CHROME_BROWSER_UI_WEBUI_FLAGS_FLAGS_UI_H_
