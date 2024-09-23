// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_UNTRUSTED_UI_H_

#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

namespace content {
class WebUI;
}  // namespace content

class PrivacySandboxDialogUntrustedUI;

// Class that stores properties for the
// chrome-untrusted://privacy-sandbox-dialog/ WebUI.
class PrivacySandboxDialogUntrustedUIConfig
    : public content::DefaultWebUIConfig<PrivacySandboxDialogUntrustedUI> {
 public:
  PrivacySandboxDialogUntrustedUIConfig();
  ~PrivacySandboxDialogUntrustedUIConfig() override;
};

// WebUI for chrome-untrusted://privacy-sandbox-dialog/, intended to be used
// when untrusted content needs to be processed.
class PrivacySandboxDialogUntrustedUI : public ui::UntrustedWebUIController {
 public:
  explicit PrivacySandboxDialogUntrustedUI(content::WebUI* web_ui);
  PrivacySandboxDialogUntrustedUI(const PrivacySandboxDialogUntrustedUI&) =
      delete;
  PrivacySandboxDialogUntrustedUI& operator=(
      const PrivacySandboxDialogUntrustedUI&) = delete;
  ~PrivacySandboxDialogUntrustedUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_DIALOG_UNTRUSTED_UI_H_
