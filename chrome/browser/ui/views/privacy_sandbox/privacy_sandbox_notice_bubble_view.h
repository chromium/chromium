// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_BUBBLE_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class Browser;

// Content view of PrivacySandboxDialog notice UI. The bubble is anchored to
// three-dot menu. It contains image, title, description and action buttons.
class PrivacySandboxNoticeBubbleView : public views::View {
 public:
  METADATA_HEADER(PrivacySandboxNoticeBubbleView);
  explicit PrivacySandboxNoticeBubbleView(Browser* browser);

 private:
  raw_ptr<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_NOTICE_BUBBLE_VIEW_H_
