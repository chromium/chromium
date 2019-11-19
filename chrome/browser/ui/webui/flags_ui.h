// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FLAGS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FLAGS_UI_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace base {
class RefCountedMemory;
}

namespace content {
class WebUIDataSource;
}

class FlagsUI : public content::WebUIController {
 public:
  explicit FlagsUI(content::WebUI* web_ui);
  ~FlagsUI() override;

  static void AddStrings(content::WebUIDataSource* source);
  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

 private:
  base::WeakPtrFactory<FlagsUI> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FlagsUI);
};

class FlagsDeprecatedUI : public content::WebUIController {
 public:
  explicit FlagsDeprecatedUI(content::WebUI* web_ui);
  ~FlagsDeprecatedUI() override;

  static void AddStrings(content::WebUIDataSource* source);
  static bool IsDeprecatedUrl(const GURL& url);

 private:
  base::WeakPtrFactory<FlagsDeprecatedUI> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FlagsDeprecatedUI);
};
#endif  // CHROME_BROWSER_UI_WEBUI_FLAGS_UI_H_
