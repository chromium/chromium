// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/titlebar_config.h"

#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/win/mica_titlebar.h"

bool ShouldBrowserCustomDrawTitlebar(BrowserView* browser_view) {
  return !ShouldDefaultThemeUseMicaTitlebar() ||
         !ThemeServiceFactory::GetForProfile(browser_view->GetProfile())
              ->UsingSystemTheme() ||
         (!browser_view->browser()->is_type_normal() &&
          !browser_view->browser()->is_type_popup() &&
          !browser_view->browser()->is_type_devtools());
}
