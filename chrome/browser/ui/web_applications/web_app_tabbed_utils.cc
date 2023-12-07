// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_tabbed_utils.h"

#include <optional>

#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"

namespace web_app {

bool HasPinnedHomeTab(const TabStripModel* tab_strip_model) {
  if (!tab_strip_model->ContainsIndex(0))
    return false;
  return tab_strip_model->delegate()->IsForWebApp() &&
         tab_strip_model->IsTabPinned(0);
}

bool IsPinnedHomeTab(const TabStripModel* tab_strip_model, int index) {
  return HasPinnedHomeTab(tab_strip_model) && index == 0;
}

bool IsTabClosable(const TabStripModel* tab_strip_model, int index) {
  return !IsPinnedHomeTab(tab_strip_model, index) ||
         tab_strip_model->count() == 1;
}

bool IsHomeTabUrl(const Browser* browser, const GURL& url) {
  return browser && browser->app_controller() &&
         HasPinnedHomeTab(browser->tab_strip_model()) &&
         browser->app_controller()->IsUrlInHomeTabScope(url);
}

}  // namespace web_app
