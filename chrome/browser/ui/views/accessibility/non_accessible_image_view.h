// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_NON_ACCESSIBLE_IMAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_NON_ACCESSIBLE_IMAGE_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"

// ImageView that sets the "invisible" state on AXNodeData so that
// the image is not traversed by screen readers.
class NonAccessibleImageView : public views::ImageView {
  METADATA_HEADER(NonAccessibleImageView, views::ImageView)

 public:
  NonAccessibleImageView();
  NonAccessibleImageView(const NonAccessibleImageView&) = delete;
  NonAccessibleImageView& operator=(const NonAccessibleImageView&) = delete;
  ~NonAccessibleImageView() override;

 private:
  // Overridden from views::ImageView.
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_NON_ACCESSIBLE_IMAGE_VIEW_H_
