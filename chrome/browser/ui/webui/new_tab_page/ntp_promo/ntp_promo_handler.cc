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
#include "components/user_education/common/user_education_features.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Constants sent to the UI to ensure that the correct promo is shown and the
// correct metrics are recorded. The distinction between empty and disabled is
// that empty means that promos would have been shown, whereas disabled
// indicates that no promo is allowed for the current page.
constexpr std::string_view kSimpleBrowserPromo = "simple";
constexpr std::string_view kEmptyBrowserPromo = "empty";
constexpr std::string_view kDisabledBrowserPromo = "disabled";

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

// static
std::string_view NtpPromoHandler::GetNtpPromoType(
    Profile* profile,
    content::WebContents* web_contents) {
  auto* controller = UserEducationServiceFactory::GetForBrowserContext(profile)
                         ->ntp_promo_controller();
  if (!controller) {
    return kDisabledBrowserPromo;
  }
  auto* user_education =
      BrowserUserEducationInterface::MaybeGetForWebContentsInTab(web_contents);
  if (!user_education) {
    return kDisabledBrowserPromo;
  }
  auto context =
      user_education->GetUserEducationContext(base::PassKey<NtpPromoHandler>());

  switch (user_education::features::GetNtpBrowserPromoType()) {
    case user_education::features::NtpBrowserPromoType::kSimple:
      return controller->HasShowablePromo(context) ? kSimpleBrowserPromo
                                                   : kEmptyBrowserPromo;
    case user_education::features::NtpBrowserPromoType::kNone:
      return kDisabledBrowserPromo;
  }
}

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
