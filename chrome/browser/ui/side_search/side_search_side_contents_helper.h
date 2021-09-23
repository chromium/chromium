// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_SIDE_CONTENTS_HELPER_H_
#define CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_SIDE_CONTENTS_HELPER_H_

#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}  // namespace content

// Side Search helper for the WebContents hosted in the side panel.
class SideSearchSideContentsHelper
    : public content::WebContentsUserData<SideSearchSideContentsHelper> {
 public:
  ~SideSearchSideContentsHelper() override;

 private:
  friend class content::WebContentsUserData<SideSearchSideContentsHelper>;
  explicit SideSearchSideContentsHelper(content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_SIDE_CONTENTS_HELPER_H_
