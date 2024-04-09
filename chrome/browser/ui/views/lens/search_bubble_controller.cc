// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/search_bubble_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/lens/search_bubble_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"

namespace lens {

SearchBubbleController::~SearchBubbleController() = default;

void SearchBubbleController::Show() {
  webui_bubble_manager_->ShowBubble(
      std::nullopt, views::BubbleBorder::TOP_RIGHT, kLensSearchBubbleElementId);
}

SearchBubbleController::SearchBubbleController(Browser* browser)
    : BrowserUserData<SearchBubbleController>(*browser),
      webui_bubble_manager_(WebUIBubbleManager::Create<SearchBubbleUI>(
          BrowserView::GetBrowserViewForBrowser(browser)->toolbar(),
          browser->profile(),
          GURL(chrome::kChromeUILensSearchBubbleURL),
          IDS_LENS_SEARCH_BUBBLE_DIALOG_TITLE,
          /*force_load_on_create=*/true)) {}

BROWSER_USER_DATA_KEY_IMPL(SearchBubbleController);

}  // namespace lens
