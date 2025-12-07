// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_UNTRUSTED_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_page_handler.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_untrusted_ui.mojom.h"
#include "content/public/browser/webui_config.h"
#include "ui/webui/untrusted_web_ui_controller.h"

class NtpMicrosoftAuthUntrustedUI;
class Profile;

class NtpMicrosoftAuthUntrustedUIConfig
    : public content::DefaultWebUIConfig<NtpMicrosoftAuthUntrustedUI> {
 public:
  NtpMicrosoftAuthUntrustedUIConfig();
  ~NtpMicrosoftAuthUntrustedUIConfig() override = default;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class NtpMicrosoftAuthUntrustedUI
    : public ui::UntrustedWebUIController,
      public new_tab_page::mojom::
          MicrosoftAuthUntrustedDocumentInterfacesFactory {
 public:
  explicit NtpMicrosoftAuthUntrustedUI(content::WebUI* web_ui);
  NtpMicrosoftAuthUntrustedUI(const NtpMicrosoftAuthUntrustedUI&) = delete;
  NtpMicrosoftAuthUntrustedUI& operator=(const NtpMicrosoftAuthUntrustedUI&) =
      delete;
  ~NtpMicrosoftAuthUntrustedUI() override;

  void BindInterface(
      mojo::PendingReceiver<
          new_tab_page::mojom::MicrosoftAuthUntrustedDocumentInterfacesFactory>
          factory);

 private:
  // new_tab_page::mojom::MicrosoftAuthUntrustedDocumentInterfacesFactory:
  void CreatePageHandler(
      mojo::PendingReceiver<
          new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler> handler,
      mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
          document) override;
  void ConnectToParentDocument(
      mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
          child_untrusted_document_remote) override;

  mojo::Receiver<
      new_tab_page::mojom::MicrosoftAuthUntrustedDocumentInterfacesFactory>
      untrusted_page_factory_{this};
  std::unique_ptr<MicrosoftAuthUntrustedPageHandler> page_handler_;

  raw_ptr<Profile> profile_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_UNTRUSTED_UI_H_
