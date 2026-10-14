// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo_handler.h"

#include <iterator>
#include <memory>

#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom-forward.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"
#include "components/user_education/common/user_education_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void CheckController(user_education::NtpPromoController* controller) {
  CHECK(controller)
      << "Should never show in a context where NTP promos are prohibited.";
}

}  // namespace

NtpPromoHandler::NtpPromoHandler(
    mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> pending_client,
    mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandler> pending_handler,
    content::WebContents* web_contents)
    : NtpPromoHandler(std::move(pending_client),
                      std::move(pending_handler),
                      web_contents,
                      UserEducationServiceFactory::GetForBrowserContext(
                          web_contents->GetBrowserContext())
                          ->ntp_promo_controller()) {}

NtpPromoHandler::NtpPromoHandler(
    mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> pending_client,
    mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandler> pending_handler,
    content::WebContents* web_contents,
    user_education::NtpPromoController* promo_controller)
    : remote_client_(std::move(pending_client)),
      receiver_(this, std::move(pending_handler)),
      web_contents_(web_contents),
      promo_controller_(promo_controller) {}

NtpPromoHandler::~NtpPromoHandler() = default;

user_education::UserEducationContextPtr NtpPromoHandler::GetContext() const {
  if (auto* user_education =
          BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
              web_contents_)) {
    return user_education->GetUserEducationContext(
        base::PassKey<NtpPromoHandler>());
  }
  return nullptr;
}

void NtpPromoHandler::RequestPromos() {
  CheckController(promo_controller_);
  const auto showable = promo_controller_->GenerateShowablePromo(GetContext());
  remote_client_->SetPromo(showable);
}

void NtpPromoHandler::OnPromoClicked(const std::string& promo_id) {
  CheckController(promo_controller_);
  promo_controller_->OnPromoClicked(promo_id, GetContext());
}

void NtpPromoHandler::OnPromoDismissed(const std::string& promo_id) {
  promo_controller_->OnPromoDismissed(promo_id);
}

void NtpPromoHandler::OnPromoShown(const std::string& promo_id) {
  promo_controller_->OnPromoShown(promo_id);
}
