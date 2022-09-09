// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/color_provider_browser_helper.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"

void ColorProviderBrowserHelper::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() != TabStripModelChange::kInserted)
    return;
  for (const auto& contents : change.GetInsert()->contents) {
    DCHECK(tab_strip_model->ContainsIndex(contents.index));
    contents.contents->SetColorProviderSource(
        BrowserView::GetBrowserViewForBrowser(&GetBrowser())->GetWidget());
  }
}

ColorProviderBrowserHelper::ColorProviderBrowserHelper(Browser* browser)
    : BrowserUserData<ColorProviderBrowserHelper>(*browser) {
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  // No WebContents should have been added to the TabStripModel before
  // ColorProviderBrowserHelper is constructed.
  DCHECK(tab_strip_model->empty());
  tab_strip_model->AddObserver(this);
}

BROWSER_USER_DATA_KEY_IMPL(ColorProviderBrowserHelper);
