// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/companion/companion_tab_helper.h"

#include <string>

#include "base/strings/strcat.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/mojom/companion.mojom.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/side_panel/companion/companion_side_panel_controller_utils.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"
#include "components/lens/buildflags.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_url_utils.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/common/translate_constants.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"

#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
#include "chrome/browser/lens/region_search/lens_region_search_controller.h"
#endif

namespace companion {

CompanionTabHelper::CompanionTabHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<CompanionTabHelper>(*web_contents),
      delegate_(CreateDelegate(web_contents)) {}

CompanionTabHelper::~CompanionTabHelper() = default;

void CompanionTabHelper::ShowCompanionSidePanelForSearchURL(
    const GURL& search_url) {
  CHECK(delegate_);
  SetTextQuery(GetTextQueryFromSearchUrl(search_url));
  delegate_->ShowCompanionSidePanel();
}

void CompanionTabHelper::ShowCompanionSidePanelForImage(
    const GURL& src_url,
    const bool is_image_translate,
    const std::string& additional_query_params_modified,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size,
    const gfx::Size& downscaled_size,
    const std::string& image_extension,
    const std::string& content_type) {
  CHECK(delegate_);

  // Create upload URL to load in companion.
  std::string upload_url_string =
      companion::features::kImageUploadURLForCompanion.Get();
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
  delegate_->ShowCompanionSidePanel();
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

std::unique_ptr<side_panel::mojom::ImageQuery>
CompanionTabHelper::GetImageQuery() {
  return std::move(image_query_);
}

std::string CompanionTabHelper::GetTextQuery() {
  std::string copy = text_query_;
  text_query_.clear();
  return copy;
}

void CompanionTabHelper::SetTextQuery(const std::string& text_query) {
  CHECK(!text_query.empty());
  text_query_ = text_query;
  if (companion_page_handler_) {
    companion_page_handler_->OnSearchTextQuery(GetTextQuery());
  }
}

void CompanionTabHelper::UpdateNewTabButtonState() {
  delegate_->UpdateNewTabButtonState();
}

GURL CompanionTabHelper::GetNewTabButtonUrl() {
  return companion_page_handler_ ? companion_page_handler_->GetNewTabButtonUrl()
                                 : GURL();
}

std::string CompanionTabHelper::GetTextQueryFromSearchUrl(
    const GURL& search_url) const {
  std::string text_query_param_value;
  if (!net::GetValueForKeyInQuery(search_url, "q", &text_query_param_value)) {
    return std::string();
  }
  return text_query_param_value;
}

void CompanionTabHelper::StartRegionSearch(content::WebContents* web_contents,
                                           bool use_fullscreen_capture) {
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
  // TODO(shaktisahu): Pass a UI entry point for accurate metrics.
  Browser* browser = companion::GetBrowserForWebContents(web_contents);
  CHECK(browser);
  if (!lens_region_search_controller_) {
    lens_region_search_controller_ =
        std::make_unique<lens::LensRegionSearchController>(browser);
  }
  lens_region_search_controller_->Start(web_contents, use_fullscreen_capture,
                                        /*is_google_default_search_provider=*/
                                        true);
#endif
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CompanionTabHelper);

}  // namespace companion
