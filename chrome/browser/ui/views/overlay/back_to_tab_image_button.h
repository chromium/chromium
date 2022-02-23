// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_IMAGE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_IMAGE_BUTTON_H_

#include "chrome/browser/ui/views/overlay/overlay_window_image_button.h"
#include "chrome/browser/ui/views/overlay/overlay_window_views.h"
#include "ui/base/metadata/metadata_header_macros.h"

// An image button representing a back-to-tab button.
class BackToTabImageButton : public OverlayWindowImageButton {
 public:
  METADATA_HEADER(BackToTabImageButton);

  explicit BackToTabImageButton(PressedCallback callback);
  BackToTabImageButton(const BackToTabImageButton&) = delete;
  BackToTabImageButton& operator=(const BackToTabImageButton&) = delete;
  ~BackToTabImageButton() override = default;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OVERLAY_BACK_TO_TAB_IMAGE_BUTTON_H_
