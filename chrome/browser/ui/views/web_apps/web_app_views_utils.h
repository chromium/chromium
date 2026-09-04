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

inline constexpr int kIconSize = 32;

// When pre-populating the name field (using the web app title) we
// should try to make some effort to not suggest things we know work extra
// poorly when used as filenames in the OS. This is especially problematic when
// creating apps for pages that have no title, because then the URL of the
// page will be used as a suggestion, and (if accepted by the user) the shortcut
// name will look really weird. For example, MacOS will convert a colon (:) to a
// forward-slash (/), and Windows will convert the colons to spaces. MacOS even
// goes a step further and collapses multiple consecutive forward-slashes in
// localized names into a single forward-slash. That means, using 'https://foo'
// as an example, an app with a display name of 'https/foo' is created on
// MacOS and 'https   foo' on Windows. By stripping away the schema, we will be
// greatly reducing the frequency of apps having weird names. Note: This
// does not affect user's ability to use URLs as an app name (which would
// result in a weird filename), it only restricts what we suggest as titles.
std::u16string NormalizeSuggestedAppTitle(const std::u16string& title);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_WEB_APP_VIEWS_UTILS_H_
