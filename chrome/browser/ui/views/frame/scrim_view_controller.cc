// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/scrim_view_controller.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"
#include "content/public/browser/web_contents.h"

ScrimViewController::ScrimViewController(BrowserView* browser_view)
    : browser_view_(browser_view),
      tab_strip_model_(browser_view->browser()->tab_strip_model()) {
  browser_view_->browser()->tab_strip_model()->AddObserver(this);
}

ScrimViewController::~ScrimViewController() {
  browser_view_->browser()->tab_strip_model()->RemoveObserver(this);
}

void ScrimViewController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    UpdateScrimViews();
  }
}

void ScrimViewController::TabBlockedStateChanged(content::WebContents* contents,
                                                 int index) {
  UpdateScrimViews();
}

void ScrimViewController::OnSplitTabChanged(const SplitTabChange& change) {
  UpdateScrimViews();
}

void ScrimViewController::UpdateScrimViews() {
  for (ContentsContainerView* contents_container_view :
       browser_view_->GetContentsContainerViews()) {
    const content::WebContents* web_contents =
        contents_container_view->contents_view()->web_contents();
    if (!web_contents) {
      continue;
    }

    const int index = tab_strip_model_->GetIndexOfWebContents(web_contents);
    if (tab_strip_model_->ContainsIndex(index)) {
      contents_container_view->contents_scrim_view()->SetVisible(
          tab_strip_model_->IsTabBlocked(index));
    }
  }
}
