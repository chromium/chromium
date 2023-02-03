// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace ash::reporting {

// The WebUI for chrome://enterprise-reporting
class EnterpriseReportingUI : public content::WebUIController {
 public:
  explicit EnterpriseReportingUI(content::WebUI* web_ui);
  ~EnterpriseReportingUI() override;
};

}  // namespace ash::reporting

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_UI_H_
