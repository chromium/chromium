// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_UI_HELPERS_H_
#define CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_UI_HELPERS_H_

#include <memory>
#include <string>

class Profile;
class GURL;

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

// Returns a human-readable string for use in titles of dialogs. Shows the drive
// letter of a path if it is the root of a file system. Elides `path` to fit
// within a standard dialog, prioritizing the file extension. See
// https://crbug.com/1354505 for context.
std::u16string GetElidedPathForDisplayAsTitle(const base::FilePath& path);
// Same as above, but does not elide `path`. This should only be used when it is
// safe to show a path which may overflow its container and have the path cut
// off (i.e. the site has already granted access to the file) or where extra
// characters would spill to the next line rather than be cut off (such as a
// dialog paragraph). See https://crbug.com/1354505 for context.
std::u16string GetPathForDisplayAsParagraph(const base::FilePath& path);

// Returns the displayable URL identity. For most URLs, it'll be the formatted
// origin. For Isolated Web Apps and Extensions, it will be their name.
std::u16string GetUrlIdentityName(Profile* profile, const GURL& url);

}  // namespace file_system_access_ui_helper

#endif  // CHROME_BROWSER_UI_VIEWS_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_UI_HELPERS_H_
