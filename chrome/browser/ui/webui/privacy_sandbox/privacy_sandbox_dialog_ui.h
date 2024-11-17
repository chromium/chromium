// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_UI_H_

#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class Profile;

namespace content {
class WebUIDataSource;
}

class PrivacySandboxDialogUI;

class PrivacySandboxDialogUIConfig
    : public content::DefaultWebUIConfig<PrivacySandboxDialogUI> {
 public:
  PrivacySandboxDialogUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIPrivacySandboxDialogHost) {}
};

// WebUI which is shown to the user as part of the PrivacySandboxDialog.
class PrivacySandboxDialogUI : public content::WebUIController {
 public:
  explicit PrivacySandboxDialogUI(content::WebUI* web_ui);
  ~PrivacySandboxDialogUI() override;

  void Initialize(Profile* profile,
                  base::OnceClosure close_callback,
                  base::OnceCallback<void(int)> resize_callback,
                  base::OnceClosure show_dialog_callback,
                  base::OnceClosure open_settings_callback,
                  base::OnceClosure open_measurement_settings_callback,
                  PrivacySandboxService::PromptType prompt_type);

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  void InitializeForDebug(content::WebUIDataSource* source);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_UI_H_
