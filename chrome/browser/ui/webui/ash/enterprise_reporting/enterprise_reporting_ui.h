// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_UI_H_

#include "ash/webui/common/chrome_os_webui_config.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting.mojom.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace ash::reporting {

class EnterpriseReportingUI;

// WebUIConfig for chrome://enterprise-reporting
class EnterpriseReportingUIConfig
    : public ChromeOSWebUIConfig<EnterpriseReportingUI> {
 public:
  EnterpriseReportingUIConfig()
      : ChromeOSWebUIConfig(content::kChromeUIScheme,
                            chrome::kChromeUIEnterpriseReportingHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI for chrome://enterprise-reporting
class EnterpriseReportingUI
    : public ui::MojoWebUIController,
      public enterprise_reporting::mojom::PageHandlerFactory {
 public:
  explicit EnterpriseReportingUI(content::WebUI* web_ui);
  ~EnterpriseReportingUI() override;

  void BindInterface(
      mojo::PendingReceiver<enterprise_reporting::mojom::PageHandlerFactory>
          receiver);

 private:
  // enterprise_reporting::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<enterprise_reporting::mojom::Page> page,
      mojo::PendingReceiver<enterprise_reporting::mojom::PageHandler> receiver)
      override;

  std::unique_ptr<EnterpriseReportingPageHandler, base::OnTaskRunnerDeleter>
      page_handler_{nullptr, base::OnTaskRunnerDeleter(nullptr)};

  mojo::Receiver<enterprise_reporting::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};
}  // namespace ash::reporting

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_UI_H_
