// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"

#include "build/build_config.h"
#include "chrome/browser/companion/core/companion_metrics_logger.h"
#include "chrome/browser/companion/core/companion_permission_utils.h"
#include "chrome/browser/companion/core/companion_url_builder.h"
#include "chrome/browser/companion/core/promo_handler.h"
#include "chrome/browser/companion/core/signin_delegate.h"
#include "chrome/browser/companion/text_finder/text_finder_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/side_panel/companion/companion_side_panel_controller_utils.h"
#include "chrome/browser/ui/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/lens/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace companion {

CompanionPageHandler::CompanionPageHandler(
    mojo::PendingReceiver<side_panel::mojom::CompanionPageHandler> receiver,
    mojo::PendingRemote<side_panel::mojom::CompanionPage> page,
    CompanionSidePanelUntrustedUI* companion_untrusted_ui)
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
  ukm::SourceId ukm_source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  metrics_logger_ = std::make_unique<CompanionMetricsLogger>(ukm_source_id);

  // Only notify the companion UI the page changed if we can share
  // information about the page by user consent.
  if (!IsUserPermittedToSharePageInfoWithCompanion(GetProfile()->GetPrefs())) {
    return;
  }
  NotifyURLChanged(/*is_full_reload=*/false);
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
    ukm::SourceId ukm_source_id =
        web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    metrics_logger_ = std::make_unique<CompanionMetricsLogger>(ukm_source_id);
    auto* helper =
        companion::CompanionTabHelper::FromWebContents(active_web_contents);
    helper->SetCompanionPageHandler(weak_ptr_factory_.GetWeakPtr());
    std::string initial_text_query = helper->GetTextQuery();
    if (!initial_text_query.empty()) {
      OnSearchTextQuery(initial_text_query);
      return;
    }

    std::unique_ptr<side_panel::mojom::ImageQuery> image_query =
        helper->GetImageQuery();
    if (image_query) {
      OnImageQuery(*image_query);
      return;
    }

    NotifyURLChanged(/*is_full_reload=*/true);
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
  page_->LoadCompanionPage(companion_url);
}

void CompanionPageHandler::NotifyURLChanged(bool is_full_reload) {
  if (is_full_reload) {
    GURL companion_url =
        url_builder_->BuildCompanionURL(web_contents()->GetVisibleURL());
    page_->LoadCompanionPage(companion_url);
  } else {
    auto companion_update_proto = url_builder_->BuildCompanionUrlParamProto(
        web_contents()->GetVisibleURL());
    page_->UpdateCompanionPage(companion_update_proto);
  }
}

void CompanionPageHandler::OnImageQuery(
    side_panel::mojom::ImageQuery image_query) {
  GURL modified_upload_url = url_builder_->AppendCompanionParamsToURL(
      image_query.upload_url, web_contents()->GetVisibleURL(),
      /*text_query=*/"");
  image_query.upload_url = modified_upload_url;
  page_->OnImageQuery(image_query.Clone());
}

GURL CompanionPageHandler::GetNewTabButtonUrl() {
  return open_in_new_tab_url_;
}

void CompanionPageHandler::OnPromoAction(
    side_panel::mojom::PromoType promo_type,
    side_panel::mojom::PromoAction promo_action) {
  promo_handler_->OnPromoAction(promo_type, promo_action);
  metrics_logger_->OnPromoAction(promo_type, promo_action);
}

void CompanionPageHandler::OnRegionSearchClicked() {
  auto* helper = companion::CompanionTabHelper::FromWebContents(web_contents());
  CHECK(helper);
  helper->StartRegionSearch(web_contents(), /*use_fullscreen_capture=*/false);
}

void CompanionPageHandler::OnExpsOptInStatusAvailable(bool is_exps_opted_in) {
  auto* pref_service = GetProfile()->GetPrefs();
  pref_service->SetBoolean(kExpsOptInStatusGrantedPref, is_exps_opted_in);
  // Update default value for pref indicating whether companion should be
  // pinned to the toolbar.
  companion::UpdateCompanionDefaultPinnedToToolbarState(pref_service);
}

void CompanionPageHandler::OnOpenInNewTabButtonURLChanged(
    const ::GURL& url_to_open) {
  auto* companion_helper =
      companion::CompanionTabHelper::FromWebContents(web_contents());
  DCHECK(companion_helper);
  open_in_new_tab_url_ = url_to_open;
  companion_helper->UpdateNewTabButtonState();
}

void CompanionPageHandler::RecordUiSurfaceShown(
    side_panel::mojom::UiSurface ui_surface,
    uint32_t child_element_count) {
  metrics_logger_->RecordUiSurfaceShown(ui_surface, child_element_count);
}

void CompanionPageHandler::RecordUiSurfaceClicked(
    side_panel::mojom::UiSurface ui_surface) {
  metrics_logger_->RecordUiSurfaceClicked(ui_surface);
}

void CompanionPageHandler::OnCqCandidatesAvailable(
    const std::vector<std::string>& text_directives) {
  auto* text_finder_manager =
      TextFinderManager::GetForPage(web_contents()->GetPrimaryPage());
  CHECK(text_finder_manager);
  text_finder_manager->CreateTextFinders(
      text_directives,
      base::BindOnce(&CompanionPageHandler::DidFinishFindingCqTexts,
                     weak_ptr_factory_.GetWeakPtr()));
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

void CompanionPageHandler::DidFinishFindingCqTexts(
    const std::vector<std::pair<std::string, bool>>& text_found_vec) {
  std::vector<std::string> text_directives(text_found_vec.size(), "");
  std::vector<bool> find_results(text_found_vec.size(), false);
  for (size_t i = 0; i < text_found_vec.size(); i++) {
    const auto& text_found = text_found_vec[i];
    text_directives[i] = text_found.first;
    find_results[i] = text_found.second;
  }
  page_->OnCqFindTextResultsAvailable(text_directives, find_results);
}

}  // namespace companion
