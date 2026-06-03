// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_VIEWS_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_VIEWS_UTILS_H_

#include <memory>
#include <string>

class GURL;

namespace base {
class Version;
}  // namespace base

namespace views {
class Label;
}  // namespace views

namespace web_app {

// Returns a label containing the app name that is suitable for presentation
// in dialogs/bubbles that require user interaction.
std::unique_ptr<views::Label> CreateNameLabel(const std::u16string& name);

// Returns a label containing the app origin that is suitable for presentation
// in dialogs/bubbles that require user interaction.
std::unique_ptr<views::Label> CreateOriginLabelFromStartUrl(
    const GURL& start_url,
    bool is_primary_text);

// Returns a label containing the app version that is suitable for presentation
// in dialogs/bubbles that require user interaction.
std::unique_ptr<views::Label> CreateVersionLabel(const base::Version& version);

// Returns a label containing the parent app name of a sub app
// that is suitable for presentation
// in dialogs/bubbles that require user interaction.
std::unique_ptr<views::Label> CreateParentNameLabel(const std::u16string& name);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_VIEWS_UTILS_H_
