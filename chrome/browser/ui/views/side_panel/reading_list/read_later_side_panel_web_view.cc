// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/reading_list/read_later_side_panel_web_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_scope.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

using SidePanelWebUIViewT_ReadingListUI = SidePanelWebUIViewT<ReadingListUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_ReadingListUI, SidePanelWebUIViewT)
END_METADATA

ReadLaterSidePanelWebView::ReadLaterSidePanelWebView(
    Profile* profile,
    SidePanelEntryScope& scope,
    base::RepeatingClosure close_cb)
    : SidePanelWebUIViewT(
          scope,
          base::BindRepeating(
              &ReadLaterSidePanelWebView::UpdateActiveURLToActiveTab,
              base::Unretained(this)),
          close_cb,
          std::make_unique<WebUIContentsWrapperT<ReadingListUI>>(
              GURL(chrome::kChromeUIReadLaterURL),
              profile,
              IDS_READ_LATER_TITLE,
              /*esc_closes_ui=*/false)) {
  SetProperty(views::kElementIdentifierKey,
              kReadLaterSidePanelWebViewElementId);
  extensions::BookmarkManagerPrivateDragEventRouter::CreateForWebContents(
      contents_wrapper()->web_contents());
  BrowserWindowInterface& browser = scope.GetBrowserWindowInterface();
  active_tab_subscription_ = browser.RegisterActiveTabDidChange(
      base::BindRepeating(&ReadLaterSidePanelWebView::OnActiveTabChanged,
                          base::Unretained(this)));
  UpdateActiveTabSubscription(browser.GetActiveTabInterface());
}

ReadLaterSidePanelWebView::~ReadLaterSidePanelWebView() = default;

void ReadLaterSidePanelWebView::UpdateActiveURL(
    content::WebContents* contents) {
  auto* controller = contents_wrapper()->GetWebUIController();
  if (!controller || !contents) {
    return;
  }

  controller->GetAs<ReadingListUI>()->SetActiveTabURL(
      chrome::GetURLToBookmark(contents));
}

void ReadLaterSidePanelWebView::UpdateActiveURLToActiveTab() {
  BrowserWindowInterface* const browser =
      webui::GetBrowserWindowInterface(web_contents());
  if (!browser) {
    return;
  }
  if (tabs::TabInterface* const active_tab = browser->GetActiveTabInterface()) {
    UpdateActiveURL(active_tab->GetContents());
  }
}

void ReadLaterSidePanelWebView::OnActiveTabChanged(
    BrowserWindowInterface* browser) {
  tabs::TabInterface* const active_tab = browser->GetActiveTabInterface();
  UpdateActiveTabSubscription(active_tab);
  if (GetVisible() && active_tab) {
    UpdateActiveURL(active_tab->GetContents());
  }
}

void ReadLaterSidePanelWebView::UpdateActiveTabSubscription(
    tabs::TabInterface* tab) {
  tab_ui_subscription_ = base::CallbackListSubscription();
  if (!tab) {
    return;
  }
  if (TabUIHelper* const tab_ui_helper = TabUIHelper::From(tab)) {
    tab_ui_subscription_ = tab_ui_helper->AddTabUIChangeCallback(
        base::BindRepeating(&ReadLaterSidePanelWebView::OnTabUIChanged,
                            base::Unretained(this)));
  }
}

void ReadLaterSidePanelWebView::OnTabUIChanged() {
  if (GetVisible()) {
    UpdateActiveURLToActiveTab();
  }
}

BEGIN_METADATA(ReadLaterSidePanelWebView)
END_METADATA
