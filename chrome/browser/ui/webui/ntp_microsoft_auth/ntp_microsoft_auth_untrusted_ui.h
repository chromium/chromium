// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_UNTRUSTED_UI_H_

#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

class NtpMicrosoftAuthUntrustedUI;

class NtpMicrosoftAuthUntrustedUIConfig
    : public content::DefaultWebUIConfig<NtpMicrosoftAuthUntrustedUI> {
 public:
  NtpMicrosoftAuthUntrustedUIConfig();
  ~NtpMicrosoftAuthUntrustedUIConfig() override = default;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class NtpMicrosoftAuthUntrustedUI : public ui::UntrustedWebUIController {
 public:
  explicit NtpMicrosoftAuthUntrustedUI(content::WebUI* web_ui);
  NtpMicrosoftAuthUntrustedUI(const NtpMicrosoftAuthUntrustedUI&) = delete;
  NtpMicrosoftAuthUntrustedUI& operator=(const NtpMicrosoftAuthUntrustedUI&) =
      delete;
  ~NtpMicrosoftAuthUntrustedUI() override;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_UNTRUSTED_UI_H_
