// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_DROP_TARGET_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_DROP_TARGET_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
}  // namespace views

// MultiContentsDropTargetView shows a drop target view used for the drag and
// drop link interaction to create a split view.
class MultiContentsDropTargetView : public views::View,
                                    public views::AnimationDelegateViews {
  METADATA_HEADER(MultiContentsDropTargetView, views::View)

 public:
  MultiContentsDropTargetView();
  MultiContentsDropTargetView(const MultiContentsDropTargetView&) = delete;
  MultiContentsDropTargetView& operator=(const MultiContentsDropTargetView&) =
      delete;
  ~MultiContentsDropTargetView() override;

  // views::View:
  void OnThemeChanged() override;

  double GetAnimationValue() const;

  void Show();
  void Hide();

  bool IsClosing() const;

  raw_ptr<views::ImageView> icon_view_for_testing() { return icon_view_; }
  gfx::SlideAnimation& animation_for_testing() { return animation_; }

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  void UpdateVisibility(bool should_be_open);

  bool ShouldShowAnimation() const;

  // Animation controlling showing and hiding of the drop target view.
  gfx::SlideAnimation animation_{this};

  raw_ptr<views::View> inner_container_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_MULTI_CONTENTS_DROP_TARGET_VIEW_H_
