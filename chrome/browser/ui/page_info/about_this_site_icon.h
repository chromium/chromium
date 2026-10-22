// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_INFO_ABOUT_THIS_SITE_ICON_H_
#define CHROME_BROWSER_UI_PAGE_INFO_ABOUT_THIS_SITE_ICON_H_

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace page_info {

// Returns the icon for the "About this site" entry points: the page info
// bubble button and the side panel action.
const gfx::VectorIcon& GetAboutThisSiteIcon();

}  // namespace page_info

#endif  // CHROME_BROWSER_UI_PAGE_INFO_ABOUT_THIS_SITE_ICON_H_
