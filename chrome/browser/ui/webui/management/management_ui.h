// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_UI_H_

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/resource/resource_scale_factor.h"

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

  ManagementUI(const ManagementUI&) = delete;
  ManagementUI& operator=(const ManagementUI&) = delete;

  ~ManagementUI() override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

  static std::u16string GetManagementPageSubtitle(Profile* profile);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MANAGEMENT_MANAGEMENT_UI_H_
