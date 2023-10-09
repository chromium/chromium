// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/shopping_bubble/shopping_prompt_impl.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/public/browser/web_contents.h"

ShoppingPromptImpl::ShoppingPromptImpl(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

void ShoppingPromptImpl::ShowDiscountConsent() {
  // TODO(meiliang): GetLocationBarView and show the shopping chip
}

LocationBarView* ShoppingPromptImpl::GetLocationBarView() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents_);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);

  return browser_view ? browser_view->GetLocationBarView() : nullptr;
}
