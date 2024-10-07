// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/commerce/shopping_ui_handler_delegate.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/webui/commerce/product_specifications_disclosure_dialog.h"
#include "chrome/browser/ui/webui/commerce/shopping_insights_side_panel_ui.h"
#include "chrome/common/url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_utils.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/webui/shopping_service_handler.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "shopping_ui_handler_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"

namespace commerce {

ShoppingUiHandlerDelegate::ShoppingUiHandlerDelegate(
    ShoppingInsightsSidePanelUI* insights_side_panel_ui,
    Profile* profile)
    : insights_side_panel_ui_(insights_side_panel_ui),
      profile_(profile),
      bookmark_model_(BookmarkModelFactory::GetForBrowserContext(profile)) {}

ShoppingUiHandlerDelegate::~ShoppingUiHandlerDelegate() = default;

std::optional<GURL> ShoppingUiHandlerDelegate::GetCurrentTabUrl() {
  auto* browser = chrome::FindTabbedBrowser(profile_, false);
  if (!browser) {
    return std::nullopt;
  }

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return std::nullopt;
  }
  return std::make_optional<GURL>(web_contents->GetLastCommittedURL());
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
    const bookmarks::BookmarkNode* parent =
        commerce::GetShoppingCollectionBookmarkFolder(bookmark_model_, true);

    return bookmark_model_->AddNewURL(parent, parent->children().size(), title,
                                      url);
  }
  return nullptr;
}

void ShoppingUiHandlerDelegate::OpenUrlInNewTab(const GURL& url) {
  auto* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  NavigateToUrl(browser, url);
}

void ShoppingUiHandlerDelegate::SwitchToOrOpenTab(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  auto* browser = chrome::FindBrowserWithActiveWindow();
  if (!browser) {
    browser = chrome::FindLastActive();
  }
  if (!browser) {
    return;
  }

  auto* tab_strip_model = browser->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    auto* web_contents = tab_strip_model->GetWebContentsAt(i);
    if (web_contents->GetLastCommittedURL() == url) {
      tab_strip_model->ActivateTabAt(i);
      return;
    }
  }

  NavigateToUrl(browser, url);
}

void ShoppingUiHandlerDelegate::ShowFeedbackForPriceInsights() {
  auto* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  chrome::ShowFeedbackPage(
      browser, feedback::kFeedbackSourcePriceInsights,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(IDS_SHOPPING_INSIGHTS_FEEDBACK_FORM_TITLE),
      /*category_tag=*/"price_insights",
      /*extra_diagnostics=*/std::string());
}

void ShoppingUiHandlerDelegate::ShowFeedbackForProductSpecifications(
    const std::string& log_id) {
  auto* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  base::Value::Dict feedback_metadata;
  feedback_metadata.Set("log_id", log_id);
  chrome::ShowFeedbackPage(
      browser, feedback::kFeedbackSourceAI,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(IDS_COMPARE_FEEDBACK_PLACEHOLDER),
      /*category_tag=*/"compare",
      /*extra_diagnostics=*/std::string(),
      /*autofill_metadata=*/base::Value::Dict(), std::move(feedback_metadata));
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

ukm::SourceId ShoppingUiHandlerDelegate::GetCurrentTabUkmSourceId() {
  auto* browser = chrome::FindTabbedBrowser(profile_, false);
  if (!browser) {
    return ukm::kInvalidSourceId;
  }
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return ukm::kInvalidSourceId;
  }
  return web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
}

void ShoppingUiHandlerDelegate::ShowProductSpecificationsDisclosureDialog(
    const std::vector<GURL>& urls,
    const std::string& name,
    const std::string& set_id) {
  auto* browser = chrome::FindTabbedBrowser(profile_, false);
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return;
  }
  // Currently this method is only used to trigger the dialog which will open
  // the potential product specification set in the current tab.
  DialogArgs dialog_args(urls, name, set_id, /*in_new_tab=*/false);
  ProductSpecificationsDisclosureDialog::ShowDialog(profile_, web_contents,
                                                    std::move(dialog_args));
}

void ShoppingUiHandlerDelegate::ShowProductSpecificationsSetForUuid(
    const base::Uuid& uuid,
    bool in_new_tab) {
  const GURL product_spec_url = commerce::GetProductSpecsTabUrlForID(uuid);
  if (in_new_tab) {
    OpenUrlInNewTab(product_spec_url);
  } else {
    auto* browser = chrome::FindLastActive();
    if (!browser) {
      return;
    }
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (!web_contents) {
      return;
    }
    web_contents->GetController().LoadURL(product_spec_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                          /*extra_headers=*/std::string());
  }
}

void ShoppingUiHandlerDelegate::NavigateToUrl(Browser* browser,
                                              const GURL& url) {
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  browser->OpenURL(params, /*navigation_handle_callback=*/{});
}

void ShoppingUiHandlerDelegate::ShowSyncSetupFlow() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  signin_ui_util::EnableSyncFromSingleAccountPromo(
      profile_, account_info,
      signin_metrics::AccessPoint::ACCESS_POINT_PRODUCT_SPECIFICATIONS);
  return;
}

}  // namespace commerce
