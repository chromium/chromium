// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_OUTLINE_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_OUTLINE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

// This view draws the outline of the contents container view. Because the
// outline is drawn over the mini toolbar we cannot use a standard border.
// It's also important for the outline to be draw as a single path to avoid
// issuess with fractional display scale factors.
class ContentsContainerOutline : public views::View,
                                 public views::ViewObserver {
  METADATA_HEADER(ContentsContainerOutline, views::View)

 public:
  static constexpr int kCornerRadius = 8;
  static constexpr int kThickness = 1;
  static constexpr int kHighlightThickness = 3;

  explicit ContentsContainerOutline(views::View* mini_toolbar);
  ContentsContainerOutline(ContentsContainerOutline&) = delete;
  ContentsContainerOutline& operator=(const ContentsContainerOutline&) = delete;
  ~ContentsContainerOutline() override;

  static int GetThickness(bool is_highlighted);
  static ui::ColorId GetColor(bool is_active, bool is_highlighted);

  bool is_highlighted() const { return is_highlighted_; }

  void UpdateState(bool is_active, bool is_highlighted);

 private:
  void SetClipPath();

  // views::View
  void OnPaint(gfx::Canvas* canvas) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  raw_ptr<views::View> mini_toolbar_ = nullptr;
  bool is_active_ = false;
  bool is_highlighted_ = false;
  base::ScopedObservation<views::View, views::ViewObserver>
      view_bounds_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_CONTENTS_CONTAINER_OUTLINE_H_
