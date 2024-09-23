// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

// The base view for both camera and mic views.
class MediaView : public views::BoxLayoutView {
  METADATA_HEADER(MediaView, views::BoxLayoutView)

 public:
  explicit MediaView(bool is_subsection = false);
  MediaView(const MediaView&) = delete;
  MediaView& operator=(const MediaView&) = delete;
  ~MediaView() override;

  void RefreshSize();

 protected:
  // views::BoxLayoutView
  void ChildPreferredSizeChanged(View* child) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_MEDIA_VIEW_H_
