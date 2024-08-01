// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/companion/companion_tab_helper.h"

#include <string>

#include "base/strings/strcat.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/companion/core/utils.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/views/side_panel/companion/companion_side_panel_controller_utils.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_url_utils.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/translate_constants.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "ui/base/models/image_model.h"

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#endif

namespace companion {

CompanionTabHelper::CompanionTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<CompanionTabHelper>(*web_contents),
      delegate_(CreateDelegate(web_contents)) {
  Observe(web_contents);
}

CompanionTabHelper::~CompanionTabHelper() = default;

void CompanionTabHelper::ShowCompanionSidePanelForSearchURL(
    const GURL& search_url) {
  CHECK(delegate_);
  SetTextQuery(GetTextQueryFromSearchUrl(search_url));
  delegate_->ShowCompanionSidePanel(
      SidePanelOpenTrigger::kContextMenuSearchOption);
}

void CompanionTabHelper::ShowCompanionSidePanelForImage(
    const GURL& src_url,
    const bool is_image_translate,
    const std::string& additional_query_params_modified,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size,
    const gfx::Size& downscaled_size,
    const std::string& content_type) {
  CHECK(delegate_);

  // Create upload URL to load in companion.
  std::string upload_url_string = companion::GetImageUploadURLForCompanion();
  base::StrAppend(&upload_url_string, {"?", additional_query_params_modified});
  GURL upload_url = GURL(upload_url_string);
  CHECK(upload_url.is_valid());

  if (is_image_translate) {
    upload_url = SetImageTranslateQueryParams(upload_url);
  }

  // Construct image query object for mojom.
  auto image_query = side_panel::mojom::ImageQuery(
      upload_url, src_url, content_type, thumbnail_data, original_size.height(),
      original_size.width(), downscaled_size.height(), downscaled_size.width());
  if (companion_page_handler_) {
    // Send request immediately if page handler already exists.
    companion_page_handler_->OnImageQuery(image_query);
  } else {
    // If the companion page handler has not been built yet, store the image
    // data so the it can be retrieved later.
    image_query_ = std::make_unique<side_panel::mojom::ImageQuery>(image_query);
  }

  // Show the side panel.
  delegate_->ShowCompanionSidePanel(SidePanelOpenTrigger::kLensContextMenu);
}

GURL CompanionTabHelper::SetImageTranslateQueryParams(GURL upload_url) {
  ChromeTranslateClient* chrome_translate_client =
      ChromeTranslateClient::FromWebContents(&GetWebContents());
  if (!chrome_translate_client) {
    return upload_url;
  }
  const translate::LanguageState& language_state =
      chrome_translate_client->GetLanguageState();
  if (language_state.IsPageTranslated()) {
    if (language_state.source_language() != translate::kUnknownLanguageCode) {
      upload_url = net::AppendOrReplaceQueryParameter(
          upload_url, lens::kTranslateSourceQueryParameter,
          language_state.source_language());
    }
    if (language_state.current_language() != translate::kUnknownLanguageCode) {
      upload_url = net::AppendOrReplaceQueryParameter(
          upload_url, lens::kTranslateTargetQueryParameter,
          language_state.current_language());
    }
    upload_url = net::AppendOrReplaceQueryParameter(
        upload_url, lens::kFilterTypeQueryParameter,
        lens::kTranslateFilterTypeQueryParameterValue);
  }
  return upload_url;
}

void CompanionTabHelper::SetCompanionPageHandler(
    base::WeakPtr<CompanionPageHandler> companion_page_handler) {
  CHECK(companion_page_handler);
  companion_page_handler_ = companion_page_handler;
}

base::WeakPtr<CompanionPageHandler>
CompanionTabHelper::GetCompanionPageHandler() {
  return companion_page_handler_;
}

void CompanionTabHelper::AddCompanionFinishedLoadingCallback(
    CompanionTabHelper::CompanionLoadedCallback callback) {
  delegate_->AddCompanionFinishedLoadingCallback(std::move(callback));
}

content::WebContents* CompanionTabHelper::GetCompanionWebContentsForTesting() {
  return delegate_->GetCompanionWebContentsForTesting();  // IN-TEST
}

std::unique_ptr<side_panel::mojom::ImageQuery>
CompanionTabHelper::GetImageQuery() {
  return std::move(image_query_);
}

bool CompanionTabHelper::HasImageQuery() {
  return image_query_ != nullptr;
}

std::string CompanionTabHelper::GetTextQuery() {
  std::string copy = text_query_;
  text_query_.clear();
  return copy;
}

std::unique_ptr<base::Time> CompanionTabHelper::GetTextQueryStartTime() {
  return std::move(text_query_start_time_);
}

void CompanionTabHelper::SetTextQuery(const std::string& text_query) {
  CHECK(!text_query.empty());
  text_query_start_time_ = std::make_unique<base::Time>(base::Time::Now());
  text_query_ = text_query;
  if (companion_page_handler_) {
    companion_page_handler_->OnSearchTextQuery();
  }
}

void CompanionTabHelper::OnCompanionSidePanelClosed() {
  image_query_.reset();
  text_query_.clear();
  side_panel_open_trigger_ = std::nullopt;
  delegate_->OnCompanionSidePanelClosed();
}

void CompanionTabHelper::CreateAndRegisterEntry() {
  delegate_->CreateAndRegisterEntry();
}

void CompanionTabHelper::DeregisterEntry() {
  delegate_->DeregisterEntry();
}

void CompanionTabHelper::UpdateNewTabButton(GURL url_to_open) {
  delegate_->UpdateNewTabButton(url_to_open);
}

std::string CompanionTabHelper::GetTextQueryFromSearchUrl(
    const GURL& search_url) const {
  std::string text_query_param_value;
  if (!net::GetValueForKeyInQuery(search_url, "q", &text_query_param_value)) {
    return std::string();
  }
  return text_query_param_value;
}

void CompanionTabHelper::StartRegionSearch(
    content::WebContents* web_contents,
    bool use_fullscreen_capture,
    bool force_open_in_new_tab,
    lens::AmbientSearchEntryPoint entry_point) {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  if (!lens_region_search_controller_) {
    lens_region_search_controller_ =
        std::make_unique<lens::LensRegionSearchController>();
  }
  lens_region_search_controller_->Start(web_contents, use_fullscreen_capture,
                                        force_open_in_new_tab,
                                        /*is_google_default_search_provider=*/
                                        true, entry_point);
#endif
}

void CompanionTabHelper::SetMostRecentSidePanelOpenTrigger(
    std::optional<SidePanelOpenTrigger> side_panel_open_trigger) {
  side_panel_open_trigger_ = side_panel_open_trigger;
}

std::optional<SidePanelOpenTrigger>
CompanionTabHelper::GetAndResetMostRecentSidePanelOpenTrigger() {
  auto copy = side_panel_open_trigger_;
  side_panel_open_trigger_ = std::nullopt;
  return copy;
}

void CompanionTabHelper::DidOpenRequestedURL(
    content::WebContents* new_contents,
    content::RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  // We catch link clicks that open in a new tab, so we can open CSC in that new
  // tab.
  if (disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB ||
      disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) {
    if (!delegate_->IsCompanionShowing()) {
      return;
    }
    delegate_->SetCompanionAsActiveEntry(new_contents);
  }
}

void CompanionTabHelper::OpenContextualLensView(
    const content::OpenURLParams& params) {
  delegate_->OpenContextualLensView(params);
}

content::WebContents* CompanionTabHelper::GetLensViewWebContentsForTesting() {
  return delegate_->GetLensViewWebContentsForTesting();  // IN-TEST
}

bool CompanionTabHelper::OpenLensResultsInNewTabForTesting() {
  return delegate_->OpenLensResultsInNewTabForTesting();  // IN-TEST
}

bool CompanionTabHelper::IsLensLaunchButtonEnabledForTesting() {
  return delegate_->IsLensLaunchButtonEnabledForTesting();  // IN-TEST
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CompanionTabHelper);

}  // namespace companion
