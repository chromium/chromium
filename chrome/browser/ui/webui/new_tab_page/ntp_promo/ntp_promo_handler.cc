// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo_handler.h"

#include <iterator>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom-forward.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"
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
    content::WebContents* web_contents,
    user_education::NtpPromoController* promo_controller)
    : remote_client_(std::move(pending_client)),
      receiver_(this, std::move(pending_handler)),
      web_contents_(web_contents),
      promo_controller_(promo_controller) {}

// static
std::unique_ptr<NtpPromoHandler> NtpPromoHandler::Create(
    mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> pending_client,
    mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandler> pending_handler,
    content::WebContents* web_contents) {
  return base::WrapUnique(new NtpPromoHandler(
      std::move(pending_client), std::move(pending_handler), web_contents,
      UserEducationServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())
          ->ntp_promo_controller()));
}

// static
std::unique_ptr<NtpPromoHandler> NtpPromoHandler::CreateForTesting(
    mojo::PendingRemote<ntp_promo::mojom::NtpPromoClient> pending_client,
    mojo::PendingReceiver<ntp_promo::mojom::NtpPromoHandler> pending_handler,
    content::WebContents* web_contents,
    user_education::NtpPromoController* promo_controller) {
  return base::WrapUnique(new NtpPromoHandler(std::move(pending_client),
                                              std::move(pending_handler),
                                              web_contents, promo_controller));
}

NtpPromoHandler::~NtpPromoHandler() = default;

void NtpPromoHandler::RequestPromos() {
  CheckController(promo_controller_);
  auto* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  CHECK(profile);
  const auto promos = promo_controller_->GenerateShowablePromos(profile);
  remote_client_->SetPromos(promos.pending, promos.completed);
}

void NtpPromoHandler::OnPromoClicked(const std::string& promo_id) {
  CheckController(promo_controller_);
  auto* bwi = webui::GetBrowserWindowInterface(web_contents_);
  CHECK(bwi);
  promo_controller_->OnPromoClicked(promo_id, bwi);
}

void NtpPromoHandler::OnPromosShown(
    const std::vector<std::string>& eligible_shown,
    const std::vector<std::string>& completed_shown) {
  promo_controller_->OnPromosShown(eligible_shown, completed_shown);
}

void NtpPromoHandler::SnoozeSetupList() {
  CheckController(promo_controller_);
  promo_controller_->SetAllPromosSnoozed(true);
}

void NtpPromoHandler::UnsnoozeSetupList() {
  CheckController(promo_controller_);
  promo_controller_->SetAllPromosSnoozed(false);
}

void NtpPromoHandler::DisableSetupList() {
  CheckController(promo_controller_);
  promo_controller_->SetAllPromosDisabled(true);
}

void NtpPromoHandler::UndisableSetupList() {
  CheckController(promo_controller_);
  promo_controller_->SetAllPromosDisabled(false);
}
