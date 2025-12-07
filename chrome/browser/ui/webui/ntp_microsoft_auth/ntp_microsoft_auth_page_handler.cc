// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_page_handler.h"

#include <string>
#include <utility>

#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_untrusted_ui.mojom.h"
#include "components/search/ntp_features.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

MicrosoftAuthUntrustedPageHandler::MicrosoftAuthUntrustedPageHandler(
    mojo::PendingReceiver<
        new_tab_page::mojom::MicrosoftAuthUntrustedPageHandler> handler,
    mojo::PendingRemote<new_tab_page::mojom::MicrosoftAuthUntrustedDocument>
        document,
    Profile* profile)
    : handler_(this, std::move(handler)),
      document_(std::move(document)),
      auth_service_(MicrosoftAuthServiceFactory::GetForProfile(profile)) {
  CHECK(auth_service_);
  microsoft_auth_service_observation_.Observe(auth_service_);
}

MicrosoftAuthUntrustedPageHandler::~MicrosoftAuthUntrustedPageHandler() =
    default;

void MicrosoftAuthUntrustedPageHandler::ClearAuthData() {
  auth_service_->ClearAuthData();
}

void MicrosoftAuthUntrustedPageHandler::MaybeAcquireTokenSilent() {
  MicrosoftAuthService::AuthState auth_state = auth_service_->GetAuthState();
  if (auth_state == MicrosoftAuthService::AuthState::kNone) {
    document_->AcquireTokenSilent();
    base::UmaHistogramEnumeration("NewTabPage.MicrosoftAuth.AuthStarted",
                                  new_tab_page::mojom::AuthType::kSilent);
  }
}

void MicrosoftAuthUntrustedPageHandler::SetAccessToken(
    new_tab_page::mojom::AccessTokenPtr token) {
  auth_service_->SetAccessToken(std::move(token));
}

void MicrosoftAuthUntrustedPageHandler::SetAuthStateError(
    const std::string& error_code,
    const std::string& error_message) {
  // All authentication errors are currently treated the same:
  // the service is marked as errored, which cancels the current
  // authentication attempt and triggers UI updates to prompt
  // the user to retry authenticating.
  auth_service_->SetAuthStateError();
  base::UmaHistogramSparse("NewTabPage.MicrosoftAuth.AuthError",
                           base::PersistentHash(error_code));
  LogModuleError(ntp_features::kNtpMicrosoftAuthenticationModule,
                 error_message);
}

void MicrosoftAuthUntrustedPageHandler::OnAuthStateUpdated() {
  MaybeAcquireTokenSilent();
}
