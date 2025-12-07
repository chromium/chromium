// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_ROW_GROUPED_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_ROW_GROUPED_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/view.h"

class OmniboxPopupViewViews;

// A view that is a child of the OmniboxPopupViewViews that groups together
// OmniboxRowViews for a grouped animation.
class OmniboxRowGroupedView : public views::View,
                              public gfx::AnimationDelegate {
  METADATA_HEADER(OmniboxRowGroupedView, views::View)

 public:
  explicit OmniboxRowGroupedView(OmniboxPopupViewViews* popup_view);
  ~OmniboxRowGroupedView() override;

  // Starts the animation if the view has not animated in yet.
  void MaybeStartAnimation();
  // Returns the current height of the view as determined by the animation.
  int GetCurrentHeight() const;
  // Called when the parent popup is hidden so that the animation can be reset.
  void OnPopupHide();

  gfx::SlideAnimation* animation_for_testing() { return animation_.get(); }
  bool has_animated_for_testing() const { return has_animated; }

 private:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimateOpacity();

  // Whether the view has already animated in and should start the animation
  // again.
  bool has_animated = false;
  const raw_ptr<OmniboxPopupViewViews> popup_view_;
  std::unique_ptr<gfx::SlideAnimation> animation_;
  base::WeakPtrFactory<OmniboxRowGroupedView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_OMNIBOX_OMNIBOX_ROW_GROUPED_VIEW_H_
