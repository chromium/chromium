// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_VIEWS_HELPERS_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_VIEWS_HELPERS_H_

#include <memory>
#include <string>

namespace base {
class FilePath;
}

namespace content {
class WebContents;
}

namespace url {
class Origin;
}

namespace views {
class View;
}

namespace file_system_access_ui_helper {

// Creates and returns a label where the place holder is replaced with `origin`.
// If `show_emphasis` is true, the origin is formatted as emphasized text.
std::unique_ptr<views::View> CreateOriginLabel(
    content::WebContents* web_contents,
    int message_id,
    const url::Origin& origin,
    int text_context,
    bool show_emphasis);

// Creates and returns a label where the place holders are replaced with
// `origin` and `path`. If `show_emphasis` is true, the origin and path are
// formatted as emphasized text.
std::unique_ptr<views::View> CreateOriginPathLabel(
    content::WebContents* web_contents,
    int message_id,
    const url::Origin& origin,
    const base::FilePath& path,
    int text_context,
    bool show_emphasis);

}  // namespace file_system_access_ui_helper

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_VIEWS_HELPERS_H_
