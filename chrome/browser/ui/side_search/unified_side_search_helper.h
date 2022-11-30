// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_SEARCH_UNIFIED_SIDE_SEARCH_HELPER_H_
#define CHROME_BROWSER_UI_SIDE_SEARCH_UNIFIED_SIDE_SEARCH_HELPER_H_

class SideSearchTabContentsHelper;

namespace content {
class WebContents;
}  // namespace content

void CreateUnifiedSideSearchController(SideSearchTabContentsHelper* creator,
                                       content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_SIDE_SEARCH_UNIFIED_SIDE_SEARCH_HELPER_H_
