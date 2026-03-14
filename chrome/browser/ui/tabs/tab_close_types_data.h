// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_CLOSE_TYPES_DATA_H_
#define CHROME_BROWSER_UI_TABS_TAB_CLOSE_TYPES_DATA_H_

#include <cstdint>

#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// Stores the close types associated with a WebContents that is being closed.
// This is used to preserve the close types across unload listeners.
class TabCloseTypesData
    : public content::WebContentsUserData<TabCloseTypesData> {
 public:
  ~TabCloseTypesData() override;

  uint32_t close_types() const { return close_types_; }

 private:
  friend class content::WebContentsUserData<TabCloseTypesData>;

  explicit TabCloseTypesData(content::WebContents* web_contents,
                             uint32_t close_types);

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  uint32_t close_types_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_CLOSE_TYPES_DATA_H_
