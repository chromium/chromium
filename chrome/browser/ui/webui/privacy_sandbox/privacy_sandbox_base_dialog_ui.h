// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_BASE_DIALOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_BASE_DIALOG_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

namespace privacy_sandbox {

// MojoWebUIController for Privacy Sandbox Base Dialog
class PrivacySandboxBaseDialogUI : public ui::MojoWebUIController {
 public:
  explicit PrivacySandboxBaseDialogUI(content::WebUI* web_ui);
  PrivacySandboxBaseDialogUI(const PrivacySandboxBaseDialogUI&) = delete;
  PrivacySandboxBaseDialogUI& operator=(const PrivacySandboxBaseDialogUI&) =
      delete;

  ~PrivacySandboxBaseDialogUI() override;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

class PrivacySandboxBaseDialogUIConfig
    : public content::DefaultWebUIConfig<PrivacySandboxBaseDialogUI> {
 public:
  PrivacySandboxBaseDialogUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIPrivacySandboxBaseDialogHost) {}
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_BASE_DIALOG_UI_H_
