// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_location_bar_model_delegate.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

BrowserLocationBarModelDelegate::BrowserLocationBarModelDelegate(
    Browser* browser)
    : browser_(browser) {}

BrowserLocationBarModelDelegate::~BrowserLocationBarModelDelegate() {}

content::WebContents* BrowserLocationBarModelDelegate::GetActiveWebContents()
    const {
  return browser_->tab_strip_model()->GetActiveWebContents();
}
