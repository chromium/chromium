// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_TABBED_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_TABBED_UTILS_H_

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "url/gurl.h"

namespace web_app {

// Returns whether the web apps tab strip contains a pinned home tab.
bool HasPinnedHomeTab(TabStripModel* tab_strip_model);

// Returns whether the tab at the given index is the pinned home tab.
bool IsPinnedHomeTab(TabStripModel* tab_strip_model, int index);

// Returns whether the given launch_url should be treated as the home tab URL.
bool IsPinnedHomeTabUrl(const WebAppRegistrar& registrar,
                        const AppId& app_id,
                        GURL launch_url);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_TABBED_UTILS_H_
