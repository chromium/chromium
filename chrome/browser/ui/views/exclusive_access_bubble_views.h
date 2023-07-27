// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXCLUSIVE_ACCESS_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXCLUSIVE_ACCESS_BUBBLE_VIEWS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_hide_callback.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_observer.h"
#include "ui/views/widget/widget_observer.h"

class ExclusiveAccessBubbleViewsContext;
class GURL;
namespace gfx {
class SlideAnimation;
}
namespace views {
class View;
class Widget;
}

class SubtleNotificationView;

// ExclusiveAccessBubbleViews is responsible for showing a bubble atop the
// screen in fullscreen/mouse lock mode, telling users how to exit and providing
// a click target. The bubble auto-hides, and re-shows when the user moves to
// the screen top.
class ExclusiveAccessBubbleViews : public ExclusiveAccessBubble,
                                   public FullscreenObserver,
                                   public views::WidgetObserver {
 public:
  ExclusiveAccessBubbleViews(
      ExclusiveAccessBubbleViewsContext* context,
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type,
      bool notify_download,
      ExclusiveAccessBubbleHideCallback bubble_first_hide_callback);

  ExclusiveAccessBubbleViews(const ExclusiveAccessBubbleViews&) = delete;
  ExclusiveAccessBubbleViews& operator=(const ExclusiveAccessBubbleViews&) =
      delete;

  ~ExclusiveAccessBubbleViews() override;

  // |force_update| indicates the caller wishes to show the bubble contents
  // regardless of whether the contents have changed. |notify_download|
  // indicates if the notification should be about a new download. Note that
  // bubble_type may be an invalid one for notify_download, as we want to
  // preserve the current type.
  void UpdateContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type,
      ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
      bool notify_download,
      bool force_update);

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
  // Starts or stops polling the mouse location based on |popup_| and
  // |bubble_type_|.
  void UpdateMouseWatcher();

  // Updates |popup|'s bounds given |animation_| and |animated_attribute_|.
  void UpdateBounds();

  void UpdateViewContent(ExclusiveAccessBubbleType bubble_type);

  // Returns whether the popup is visible.
  bool IsVisible() const;

  // Returns the root view containing |browser_view_|.
  views::View* GetBrowserRootView() const;

  // ExclusiveAccessBubble:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  gfx::Rect GetPopupRect() const override;
  gfx::Point GetCursorScreenPoint() override;
  bool WindowContainsPoint(gfx::Point pos) override;
  bool IsWindowActive() override;
  void Hide() override;
  void Show() override;
  bool IsAnimating() override;
  bool CanTriggerOnMouse() const override;

  // FullscreenObserver:
  void OnFullscreenStateChanged() override;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  void RunHideCallbackIfNeeded(ExclusiveAccessBubbleHideReason reason);

  const raw_ptr<ExclusiveAccessBubbleViewsContext> bubble_view_context_;

  raw_ptr<views::Widget> popup_;

  // Classic mode: Bubble may show & hide multiple times. The callback only runs
  // for the first hide.
  // Simplified mode: Bubble only hides once.
  ExclusiveAccessBubbleHideCallback bubble_first_hide_callback_;

  // Animation controlling showing/hiding of the exit bubble.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  // The contents of the popup.
  raw_ptr<SubtleNotificationView> view_;
  std::u16string browser_fullscreen_exit_accelerator_;

  base::ScopedObservation<FullscreenController, FullscreenObserver>
      fullscreen_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXCLUSIVE_ACCESS_BUBBLE_VIEWS_H_
