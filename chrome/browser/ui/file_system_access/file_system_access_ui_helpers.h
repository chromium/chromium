// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_UI_HELPERS_H_
#define CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_UI_HELPERS_H_

#include <memory>
#include <string>

class Profile;
class GURL;

namespace content {
struct PathInfo;
}

namespace file_system_access_ui_helper {

// Returns a human-readable string for use in titles of dialogs. Uses
// `path_info.display_name` if it is non-empty and does not match
// path_info.path.BaseName(), otherwise uses `path_info.path`. Shows the drive
// letter of a path if it is the root of a file system. Elides `path` to fit
// within a standard dialog, prioritizing the file extension. See
// https://crbug.com/1354505 for context.
std::u16string GetElidedPathForDisplayAsTitle(
    const content::PathInfo& path_info);
// Same as above, but does not elide `path`. This should only be used when it is
// safe to show a path which may overflow its container and have the path cut
// off (i.e. the site has already granted access to the file) or where extra
// characters would spill to the next line rather than be cut off (such as a
// dialog paragraph). See https://crbug.com/1354505 for context.
std::u16string GetPathForDisplayAsParagraph(const content::PathInfo& path);

// Returns the displayable URL identity. For most URLs, it'll be the formatted
// origin. For Isolated Web Apps and Extensions, it will be their name.
std::u16string GetUrlIdentityName(Profile* profile, const GURL& url);

}  // namespace file_system_access_ui_helper

#endif  // CHROME_BROWSER_UI_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_UI_HELPERS_H_
