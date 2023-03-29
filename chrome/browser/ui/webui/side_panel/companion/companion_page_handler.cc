// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_url_builder.h"
#include "chrome/browser/ui/webui/side_panel/companion/msbb_delegate.h"
#include "chrome/browser/ui/webui/side_panel/companion/promo_handler.h"
#include "chrome/browser/ui/webui/side_panel/companion/signin_delegate.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/lens/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace companion {

CompanionPageHandler::CompanionPageHandler(
    mojo::PendingReceiver<side_panel::mojom::CompanionPageHandler> receiver,
    mojo::PendingRemote<side_panel::mojom::CompanionPage> page,
    Browser* browser,
    CompanionSidePanelUntrustedUI* companion_untrusted_ui)
    : content::WebContentsObserver(
          browser->tab_strip_model()->GetActiveWebContents()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      browser_(browser),
      companion_untrusted_ui_(companion_untrusted_ui),
      signin_delegate_(SigninDelegate::Create(browser->profile())),
      url_builder_(
          std::make_unique<CompanionUrlBuilder>(browser->profile()->GetPrefs(),
                                                signin_delegate_.get(),
                                                this)) {
  DCHECK(browser);
  promo_handler_ = std::make_unique<PromoHandler>(
      browser->profile()->GetPrefs(), signin_delegate_.get(), this);
  NotifyURLChanged();
}

CompanionPageHandler::~CompanionPageHandler() = default;

void CompanionPageHandler::PrimaryPageChanged(content::Page& page) {
  if (!IsMsbbEnabled()) {
    return;
  }
  NotifyURLChanged();
}

void CompanionPageHandler::ShowUI() {
  if (auto embedder = companion_untrusted_ui_->embedder()) {
    embedder->ShowUI();
  }
}

void CompanionPageHandler::NotifyURLChanged() {
  GURL companion_url =
      url_builder_->BuildCompanionURL(web_contents()->GetVisibleURL());
  page_->OnURLChanged(companion_url.spec());
}

void CompanionPageHandler::OnPromoAction(
    side_panel::mojom::PromoType promo_type,
    side_panel::mojom::PromoAction promo_action) {
  promo_handler_->OnPromoAction(promo_type, promo_action);
}

void CompanionPageHandler::OnRegionSearchClicked() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Start a region search here.
  // TODO(shaktisahu): Pass a UI entry point for accurate metrics.
  if (!lens_region_search_controller_) {
    lens_region_search_controller_ =
        std::make_unique<lens::LensRegionSearchController>(browser_);
  }
  auto* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  bool is_google_default_search_provider =
      search::DefaultSearchProviderIsGoogle(profile);
  lens_region_search_controller_->Start(web_contents(),
                                        /*use_fullscreen_capture=*/false,
                                        is_google_default_search_provider);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void CompanionPageHandler::EnableMsbb(bool enable_msbb) {
  auto* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  auto* consent_service = UnifiedConsentServiceFactory::GetForProfile(profile);
  consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(enable_msbb);
}

bool CompanionPageHandler::IsMsbbEnabled() {
  auto* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper> helper =
      unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(profile->GetPrefs());
  return helper->IsEnabled();
}

}  // namespace companion
