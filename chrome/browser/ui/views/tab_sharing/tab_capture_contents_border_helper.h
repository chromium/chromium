// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_CAPTURE_CONTENTS_BORDER_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_CAPTURE_CONTENTS_BORDER_HELPER_H_

#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// Helps track whether the contents-border should be drawn.
// TODO(crbug.com/1276822): Support dynamic borders for tabs that only
// have a single capturer.
class TabCaptureContentsBorderHelper
    : public content::WebContentsUserData<TabCaptureContentsBorderHelper> {
 public:
  ~TabCaptureContentsBorderHelper() override = default;

  void IncrementCapturerCount();
  void DecrementCapturerCount();

  void VisibilityUpdated();

 private:
  friend WebContentsUserData;

  explicit TabCaptureContentsBorderHelper(content::WebContents* web_contents)
      : content::WebContentsUserData<TabCaptureContentsBorderHelper>(
            *web_contents) {}

  void Update();

  int capturer_count_ = 0;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_CAPTURE_CONTENTS_BORDER_HELPER_H_
