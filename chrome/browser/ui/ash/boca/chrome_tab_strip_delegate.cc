// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/boca/chrome_tab_strip_delegate.h"

#include "ash/public/cpp/tab_strip_delegate.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/full_restore_utils.h"
#include "content/public/browser/web_contents.h"

ChromeTabStripDelegate::ChromeTabStripDelegate() = default;
ChromeTabStripDelegate::~ChromeTabStripDelegate() = default;

std::vector<ash::TabInfo> ChromeTabStripDelegate::GetTabsListForWindow(
    aura::Window* window) const {
  if (!window) {
    return {};
  }

  const std::string app_id = full_restore::GetAppId(window);
  // Lacros will not be supported.
  if (app_id == app_constants::kLacrosAppId) {
    return {};
  }

  // If the given `window` contains a browser frame
  auto* browser_view = BrowserView::GetBrowserViewForNativeWindow(window);

  if (!browser_view) {
    return {};
  }

  std::vector<ash::TabInfo> tabs;
  auto* tab_strip_model = browser_view->browser()->tab_strip_model();
  for (int i = 0; i < tab_strip_model->count(); i++) {
    TabRendererData tab_renderer_data =
        TabRendererData::FromTabInModel(tab_strip_model, i);
    ash::TabInfo tab;
    tab.url = tab_renderer_data.visible_url;
    tab.title = tab_renderer_data.title;
    tab.favicon = tab_renderer_data.favicon.IsEmpty()
                      ? favicon::GetDefaultFaviconModel()
                      : tab_renderer_data.favicon;

    auto* web_contents = tab_strip_model->GetWebContentsAt(i);
    tab.last_access_timetick = web_contents->GetLastActiveTimeTicks();

    tabs.push_back(tab);
  }
  return tabs;
}
