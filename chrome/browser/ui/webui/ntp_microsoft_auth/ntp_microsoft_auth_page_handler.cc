// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_page_handler.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_untrusted_ui.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace {

// Enum for use in NewTabPage.MicrosoftAuth.AuthError histogram.
// Must match the NTPMicrosoftAuthError enum.
enum class MicrosoftAuthError { kOther = 0, kMaxValue = kOther };

}  // namespace

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

void MicrosoftAuthUntrustedPageHandler::SetAuthStateError() {
  auth_service_->SetAuthStateError();
  base::UmaHistogramEnumeration("NewTabPage.MicrosoftAuth.AuthError",
                                MicrosoftAuthError::kOther);
}

void MicrosoftAuthUntrustedPageHandler::OnAuthStateUpdated() {
  MaybeAcquireTokenSilent();
}
