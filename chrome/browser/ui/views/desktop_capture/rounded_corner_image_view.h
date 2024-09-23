// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_ROUNDED_CORNER_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_ROUNDED_CORNER_IMAGE_VIEW_H_

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/image_view.h"

// TODO(crbug.com/40273922): Add support for rounded image corners to ImageView
class RoundedCornerImageView : public views::ImageView {
  METADATA_HEADER(RoundedCornerImageView, views::ImageView)

 public:
  RoundedCornerImageView() = default;
  RoundedCornerImageView(const RoundedCornerImageView&) = delete;
  RoundedCornerImageView& operator=(const RoundedCornerImageView&) = delete;

  // views::ImageView:
  bool GetCanProcessEventsWithinSubtree() const override;

 protected:
  // views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_ROUNDED_CORNER_IMAGE_VIEW_H_
