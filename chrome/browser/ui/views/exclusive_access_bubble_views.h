// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXCLUSIVE_ACCESS_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXCLUSIVE_ACCESS_BUBBLE_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_hide_callback.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/widget/widget_observer.h"

class ExclusiveAccessBubbleViewsContext;
namespace gfx {
class SlideAnimation;
}
namespace views {
class View;
class Widget;
}

class SubtleNotificationView;

// ExclusiveAccessBubbleViews is shows a bubble informing users of fullscreen,
// keyboard lock, and pointer lock modes, with instructions for exiting.
class ExclusiveAccessBubbleViews : public ExclusiveAccessBubble,
                                   public gfx::AnimationDelegate,
                                   public views::WidgetObserver {
 public:
  ExclusiveAccessBubbleViews(
      ExclusiveAccessBubbleViewsContext* context,
      const ExclusiveAccessBubbleParams& params,
      ExclusiveAccessBubbleHideCallback first_hide_callback);

  ExclusiveAccessBubbleViews(const ExclusiveAccessBubbleViews&) = delete;
  ExclusiveAccessBubbleViews& operator=(const ExclusiveAccessBubbleViews&) =
      delete;

  ~ExclusiveAccessBubbleViews() override;

  void Update(const ExclusiveAccessBubbleParams& params,
              ExclusiveAccessBubbleHideCallback first_hide_callback);

  // Repositions |popup_| if it is visible.
  void RepositionIfVisible();

  // If popup is visible, hides |popup_| before the bubble automatically hides
  // itself.
  void HideImmediately();

  // Returns true if the popup is being shown (and not fully shown).
  bool IsShowing() const;

  views::View* GetView();

  gfx::SlideAnimation* animation_for_test() { return animation_.get(); }

  bool IsVisibleForTesting() const { return IsVisible(); }

 private:
  // Updates |popup|'s bounds given |animation_| and |animated_attribute_|.
  void UpdateBounds();

  void UpdateViewContent(ExclusiveAccessBubbleType bubble_type);

  // Returns whether the popup is visible.
  bool IsVisible() const;

  // Returns the desired rect for the popup window in screen coordinates.
  gfx::Rect GetPopupRect() const;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // ExclusiveAccessBubble:
  void Hide() override;
  void Show() override;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

  void RunHideCallbackIfNeeded(ExclusiveAccessBubbleHideReason reason);

  const raw_ptr<ExclusiveAccessBubbleViewsContext> bubble_view_context_;

  raw_ptr<views::Widget> popup_;

  // Callback that runs the first time the bubble hides.
  ExclusiveAccessBubbleHideCallback first_hide_callback_;

  // Animation controlling showing/hiding of the exit bubble.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  // The contents of the popup.
  raw_ptr<SubtleNotificationView> view_;
  std::u16string browser_fullscreen_exit_accelerator_;

  // Whether the bubble was updated for a download while showing.
  bool notify_overridden_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXCLUSIVE_ACCESS_BUBBLE_VIEWS_H_
