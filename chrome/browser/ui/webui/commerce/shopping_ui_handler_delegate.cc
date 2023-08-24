// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/shopping_ui_handler_delegate.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/commerce/shopping_insights_side_panel_ui.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/webui/shopping_list_handler.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"

#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"

namespace commerce {

ShoppingUiHandlerDelegate::ShoppingUiHandlerDelegate(
    ShoppingInsightsSidePanelUI* insights_side_panel_ui,
    Profile* profile)
    : insights_side_panel_ui_(insights_side_panel_ui),
      profile_(profile),
      bookmark_model_(BookmarkModelFactory::GetForBrowserContext(profile)) {}

ShoppingUiHandlerDelegate::~ShoppingUiHandlerDelegate() = default;

absl::optional<GURL> ShoppingUiHandlerDelegate::GetCurrentTabUrl() {
  auto* browser = chrome::FindTabbedBrowser(profile_, false);
  if (!browser) {
    return absl::nullopt;
  }

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return absl::nullopt;
  }
  return absl::make_optional<GURL>(web_contents->GetLastCommittedURL());
}

void ShoppingUiHandlerDelegate::ShowInsightsSidePanelUI() {
  if (insights_side_panel_ui_) {
    auto embedder = insights_side_panel_ui_->embedder();
    if (embedder) {
      embedder->ShowUI();
    }
  }
}

const bookmarks::BookmarkNode*
ShoppingUiHandlerDelegate::GetOrAddBookmarkForCurrentUrl() {
  auto* browser = chrome::FindLastActive();
  if (!browser) {
    return nullptr;
  }
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return nullptr;
  }

  const bookmarks::BookmarkNode* existing_node =
      bookmark_model_->GetMostRecentlyAddedUserNodeForURL(
          web_contents->GetLastCommittedURL());
  if (existing_node != nullptr) {
    return existing_node;
  }
  GURL url;
  std::u16string title;
  if (chrome::GetURLAndTitleToBookmark(web_contents, &url, &title)) {
    const bookmarks::BookmarkNode* other_node = bookmark_model_->other_node();
    return bookmark_model_->AddNewURL(other_node, other_node->children().size(),
                                      title, url);
  }
  return nullptr;
}

void ShoppingUiHandlerDelegate::OpenUrlInNewTab(const GURL& url) {
  auto* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  browser->OpenURL(params);
}

void ShoppingUiHandlerDelegate::ShowFeedback() {
  auto* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  chrome::ShowFeedbackPage(
      browser, chrome::kFeedbackSourcePriceInsights,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(IDS_SHOPPING_INSIGHTS_FEEDBACK_FORM_TITLE),
      /*category_tag=*/"price_insights",
      /*extra_diagnostics=*/std::string());
}

void ShoppingUiHandlerDelegate::ShowBookmarkEditorForCurrentUrl() {
  auto current_url = GetCurrentTabUrl();
  if (!current_url.has_value()) {
    return;
  }
  auto* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }
  const bookmarks::BookmarkNode* existing_node =
      bookmark_model_->GetMostRecentlyAddedUserNodeForURL(current_url.value());
  if (!existing_node) {
    return;
  }
  BookmarkEditor::Show(browser->window()->GetNativeWindow(), profile_,
                       BookmarkEditor::EditDetails::EditNode(existing_node),
                       BookmarkEditor::SHOW_TREE);
}
}  // namespace commerce
