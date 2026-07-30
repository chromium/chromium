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
  const BrowserWindowInterface::Type type = browser_view->browser()->GetType();
  return !ShouldDefaultThemeUseMicaTitlebar() ||
         !ThemeServiceFactory::GetForProfile(browser_view->GetProfile())
              ->UsingSystemTheme() ||
         (type != BrowserWindowInterface::Type::TYPE_NORMAL &&
          type != BrowserWindowInterface::Type::TYPE_POPUP &&
          type != BrowserWindowInterface::Type::TYPE_DEVTOOLS);
}
