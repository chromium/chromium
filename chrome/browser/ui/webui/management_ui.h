// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_H_

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/resource/scale_factor.h"

namespace base {
class RefCountedMemory;
}

namespace content {
class WebUI;
}

// The Web UI controller for the chrome://management page.
class ManagementUI : public content::WebUIController {
 public:
  explicit ManagementUI(content::WebUI* web_ui);
  ~ManagementUI() override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

  static base::string16 GetManagementPageSubtitle(Profile* profile);

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagementUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MANAGEMENT_UI_H_
