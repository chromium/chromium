// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_VIEWS_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_VIEWS_UTILS_H_

#include "ui/views/controls/label.h"
#include "url/gurl.h"

namespace web_app {

// Returns a label containing the app name that is suitable for presentation
// in dialogs/bubbles that require user interaction.
std::unique_ptr<views::Label> CreateNameLabel(const std::u16string& name);

// Returns a label containing the app origin that is suitable for presentation
// in dialogs/bubbles that require user interaction.
std::unique_ptr<views::Label> CreateOriginLabelFromStartUrl(
    const GURL& start_url,
    bool is_primary_text);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_VIEWS_UTILS_H_
