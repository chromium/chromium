// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/side_panel/companion/companion_side_panel_controller_utils.h"
#include "chrome/browser/ui/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_permission_utils.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_url_builder.h"
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
    raw_ptr<CompanionSidePanelUntrustedUI> companion_untrusted_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      companion_untrusted_ui_(companion_untrusted_ui),
      signin_delegate_(SigninDelegate::Create(GetProfile())),
      url_builder_(
          std::make_unique<CompanionUrlBuilder>(GetProfile()->GetPrefs(),
                                                signin_delegate_.get())),
      promo_handler_(std::make_unique<PromoHandler>(GetProfile()->GetPrefs(),
                                                    signin_delegate_.get(),
                                                    this)) {}

CompanionPageHandler::~CompanionPageHandler() = default;

void CompanionPageHandler::PrimaryPageChanged(content::Page& page) {
  // Only notify the companion UI the page changed if we can share
  // information about the page by user consent.
  if (IsUserPermittedToSharePageInfoWithCompanion(GetProfile()->GetPrefs())) {
    return;
  }
  NotifyURLChanged();
}

void CompanionPageHandler::ShowUI() {
  if (auto embedder = companion_untrusted_ui_->embedder()) {
    embedder->ShowUI();

    // Calls to the browser need to happen after the ShowUI() call above since
    // it is only added to browser hierarchy after the side panel has loaded the
    // page.
    auto* active_web_contents =
        GetBrowser()->tab_strip_model()->GetActiveWebContents();
    Observe(active_web_contents);
    auto* helper =
        companion::CompanionTabHelper::FromWebContents(active_web_contents);
    helper->SetCompanionPageHandler(weak_ptr_factory_.GetWeakPtr());
    std::string initial_text_query = helper->GetTextQuery();
    if (initial_text_query.empty()) {
      NotifyURLChanged();
    } else {
      OnSearchTextQuery(initial_text_query);
    }
  }
}

void CompanionPageHandler::OnSearchTextQuery(const std::string& query) {
  // Only notify the companion UI the page changed if we can share
  // information about the page by user consent.
  GURL page_url;
  if (IsUserPermittedToSharePageInfoWithCompanion(GetProfile()->GetPrefs())) {
    page_url = web_contents()->GetVisibleURL();
  }

  GURL companion_url = url_builder_->BuildCompanionURL(page_url, query);
  page_->OnURLChanged(companion_url);
}

void CompanionPageHandler::NotifyURLChanged() {
  GURL companion_url =
      url_builder_->BuildCompanionURL(web_contents()->GetVisibleURL());
  page_->OnURLChanged(companion_url);
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
        std::make_unique<lens::LensRegionSearchController>(GetBrowser());
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
  auto* consent_service =
      UnifiedConsentServiceFactory::GetForProfile(GetProfile());
  consent_service->SetUrlKeyedAnonymizedDataCollectionEnabled(enable_msbb);
}

Browser* CompanionPageHandler::GetBrowser() {
  auto* webui_contents = companion_untrusted_ui_->web_ui()->GetWebContents();
  auto* browser = companion::GetBrowserForWebContents(webui_contents);
  return browser;
}

Profile* CompanionPageHandler::GetProfile() {
  CHECK(companion_untrusted_ui_);
  return Profile::FromWebUI(companion_untrusted_ui_->web_ui());
}

}  // namespace companion
