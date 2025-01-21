// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_untrusted_ui.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class MicrosoftAuthService;
class Profile;

class MicrosoftAuthUntrustedPageHandler
    : public new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler {
 public:
  MicrosoftAuthUntrustedPageHandler(
      mojo::PendingReceiver<
          new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler>
          pending_handler,
      Profile* profile);

  MicrosoftAuthUntrustedPageHandler(const MicrosoftAuthUntrustedPageHandler&) =
      delete;
  MicrosoftAuthUntrustedPageHandler& operator=(
      const MicrosoftAuthUntrustedPageHandler&) = delete;

  ~MicrosoftAuthUntrustedPageHandler() override;

  // new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler:
  void SetAccessToken(
      new_tab_page::mojom::AccessTokenPtr access_token) override;
  void SetAuthStateError() override;

 private:
  mojo::Receiver<new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler>
      handler_;
  raw_ptr<MicrosoftAuthService> auth_service_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_PAGE_HANDLER_H_
