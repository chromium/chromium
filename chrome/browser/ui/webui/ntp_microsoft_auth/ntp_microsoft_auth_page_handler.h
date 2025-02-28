// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_PAGE_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_observer.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_shared_ui.mojom.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_untrusted_ui.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class MicrosoftAuthService;
class Profile;

class MicrosoftAuthUntrustedPageHandler
    : public new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler,
      public MicrosoftAuthServiceObserver {
 public:
  MicrosoftAuthUntrustedPageHandler(
      mojo::PendingReceiver<
          new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler>
          pending_handler,
      mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
          pending_document,
      Profile* profile);

  MicrosoftAuthUntrustedPageHandler(const MicrosoftAuthUntrustedPageHandler&) =
      delete;
  MicrosoftAuthUntrustedPageHandler& operator=(
      const MicrosoftAuthUntrustedPageHandler&) = delete;

  ~MicrosoftAuthUntrustedPageHandler() override;

  // new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler:
  void ClearAuthData() override;
  void MaybeAcquireTokenSilent() override;
  void SetAccessToken(
      new_tab_page::mojom::AccessTokenPtr access_token) override;
  void SetAuthStateError(const std::string& error_code,
                         const std::string& error_message) override;

  // MicrosoftAuthServiceObserver:
  void OnAuthStateUpdated() override;

 private:
  mojo::Receiver<new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler>
      handler_;
  mojo::Remote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument> document_;
  raw_ptr<MicrosoftAuthService> auth_service_;
  base::ScopedObservation<MicrosoftAuthService, MicrosoftAuthServiceObserver>
      microsoft_auth_service_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_NTP_MICROSOFT_AUTH_NTP_MICROSOFT_AUTH_PAGE_HANDLER_H_
