// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_later_side_panel_web_view.h"

#include <memory>

#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

using SidePanelWebUIViewT_ReadingListUI = SidePanelWebUIViewT<ReadingListUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_ReadingListUI, SidePanelWebUIViewT)
END_METADATA

ReadLaterSidePanelWebView::ReadLaterSidePanelWebView(
    Browser* browser,
    base::RepeatingClosure close_cb)
    : SidePanelWebUIViewT(
          base::BindRepeating(
              &ReadLaterSidePanelWebView::UpdateActiveURLToActiveTab,
              base::Unretained(this)),
          close_cb,
          std::make_unique<WebUIContentsWrapperT<ReadingListUI>>(
              GURL(chrome::kChromeUIReadLaterURL),
              browser->profile(),
              IDS_READ_LATER_TITLE,
              /*esc_closes_ui=*/false)),
      browser_(browser) {
  SetProperty(views::kElementIdentifierKey,
              kReadLaterSidePanelWebViewElementId);
  extensions::BookmarkManagerPrivateDragEventRouter::CreateForWebContents(
      contents_wrapper()->web_contents());
  browser_->tab_strip_model()->AddObserver(this);
}

ReadLaterSidePanelWebView::~ReadLaterSidePanelWebView() = default;

void ReadLaterSidePanelWebView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (GetVisible() && selection.active_tab_changed())
    UpdateActiveURL(tab_strip_model->GetActiveWebContents());
}

void ReadLaterSidePanelWebView::TabChangedAt(content::WebContents* contents,
                                             int index,
                                             TabChangeType change_type) {
  if (GetVisible() && index == browser_->tab_strip_model()->active_index() &&
      change_type == TabChangeType::kAll) {
    UpdateActiveURL(browser_->tab_strip_model()->GetWebContentsAt(index));
  }
}

void ReadLaterSidePanelWebView::UpdateActiveURL(
    content::WebContents* contents) {
  auto* controller = contents_wrapper()->GetWebUIController();
  if (!controller || !contents)
    return;

  controller->GetAs<ReadingListUI>()->SetActiveTabURL(
      chrome::GetURLToBookmark(contents));
}

void ReadLaterSidePanelWebView::UpdateActiveURLToActiveTab() {
  UpdateActiveURL(browser_->tab_strip_model()->GetActiveWebContents());
}

BEGIN_METADATA(ReadLaterSidePanelWebView)
END_METADATA
