// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_page_handler.h"

#include <utility>

#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_untrusted_ui.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

MicrosoftAuthUntrustedPageHandler::MicrosoftAuthUntrustedPageHandler(
    mojo::PendingReceiver<
        new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler> handler,
    Profile* profile)
    : handler_(this, std::move(handler)),
      auth_service_(MicrosoftAuthServiceFactory::GetForProfile(profile)) {}

MicrosoftAuthUntrustedPageHandler::~MicrosoftAuthUntrustedPageHandler() =
    default;

void MicrosoftAuthUntrustedPageHandler::ClearAuthData() {
  if (auth_service_) {
    auth_service_->ClearAuthData();
  }
}

void MicrosoftAuthUntrustedPageHandler::SetAccessToken(
    new_tab_page::mojom::AccessTokenPtr token) {
  if (auth_service_) {
    auth_service_->SetAccessToken(std::move(token));
  }
}

void MicrosoftAuthUntrustedPageHandler::SetAuthStateError() {
  if (auth_service_) {
    auth_service_->SetAuthStateError();
  }
}
