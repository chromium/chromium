// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CAPTURE_BORDER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CAPTURE_BORDER_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class ContentsCaptureBorderView : public views::View {
  METADATA_HEADER(ContentsCaptureBorderView, views::View)
 public:
  ContentsCaptureBorderView();
  ContentsCaptureBorderView(const ContentsCaptureBorderView&) = delete;
  ContentsCaptureBorderView& operator=(const ContentsCaptureBorderView&) =
      delete;
  ~ContentsCaptureBorderView() override;

  static constexpr int kContentsBorderThickness = 5;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CAPTURE_BORDER_VIEW_H_
