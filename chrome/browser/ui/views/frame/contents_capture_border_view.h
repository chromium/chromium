// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CAPTURE_BORDER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CAPTURE_BORDER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class ContentsCaptureBorderView : public views::View,
                                  public views::ViewObserver {
  METADATA_HEADER(ContentsCaptureBorderView, views::View)
 public:
  explicit ContentsCaptureBorderView(views::View* mini_toolbar);
  ContentsCaptureBorderView(const ContentsCaptureBorderView&) = delete;
  ContentsCaptureBorderView& operator=(const ContentsCaptureBorderView&) =
      delete;
  ~ContentsCaptureBorderView() override;

  static constexpr int kContentsBorderThickness = 5;

  void SetIsInSplit(bool is_in_split);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view,
                               bool visible) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  bool is_in_split_ = false;
  raw_ptr<views::View> mini_toolbar_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver>
      view_bounds_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CAPTURE_BORDER_VIEW_H_
