// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_

#include <memory>
#include <set>

#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_embedder.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_metrics.h"
#include "chrome/common/buildflags.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if !BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#error
#endif

namespace ui {
class MenuModel;
}  // namespace ui

namespace views {
class MenuRunner;
class NativeViewHost;
class WebView;
}  // namespace views

class Browser;
class BrowserView;
enum class WebUITabStripDragDirection;
enum class WebUITabStripOpenCloseReason;
class ImmersiveRevealedLock;

class WebUITabStripContainerView : public TabStripUIEmbedder,
                                   public gfx::AnimationDelegate,
                                   public views::AccessiblePaneView,
                                   public views::ViewObserver {
 public:
  WebUITabStripContainerView(BrowserView* browser_view,
                             views::View* tab_contents_container,
                             views::View* top_container,
                             views::View* omnibox);
  ~WebUITabStripContainerView() override;

  static bool SupportsTouchableTabStrip(const Browser* browser);
  static bool UseTouchableTabStrip(const Browser* browser);

  // For drag-and-drop support:
  static void GetDropFormatsForView(
      int* formats,
      std::set<ui::ClipboardFormatType>* format_types);
  static bool IsDraggedTab(const ui::OSExchangeData& data);

  void OpenForTabDrag();

  views::NativeViewHost* GetNativeViewHost();

  // Control button. Must only be called once.
  std::unique_ptr<views::View> CreateTabCounter();

  views::View* tab_counter() const { return tab_counter_; }

  // Clicking the tab counter button opens and closes the container with
  // an animation, so it is unsuitable for an interactive test. This
  // should be called instead. View::SetVisible() isn't sufficient since
  // the container's preferred size will change.
  void SetVisibleForTesting(bool visible);
  views::WebView* web_view_for_testing() const { return web_view_; }

  // Finish the open or close animation if it's active.
  void FinishAnimationForTesting();

 private:
  class AutoCloser;
  class DragToOpenHandler;

  // Called as we are dragged open.
  bool CanStartDragToOpen(WebUITabStripDragDirection direction) const;
  void UpdateHeightForDragToOpen(float height_delta);

  // Called when drag-to-open finishes. If |fling_direction| is present,
  // the user released their touch with a high velocity. We should use
  // just this direction to animate open or closed.
  void EndDragToOpen(base::Optional<WebUITabStripDragDirection>
                         fling_direction = base::nullopt);

  void TabCounterPressed(const ui::Event& event);

  void SetContainerTargetVisibility(bool target_visible,
                                    WebUITabStripOpenCloseReason reason);

  // Passed to the AutoCloser to handle closing.
  void CloseForEventOutsideTabStrip(TabStripUICloseAction reason);

  // TabStripUI::Embedder:
  const ui::AcceleratorProvider* GetAcceleratorProvider() const override;
  void CloseContainer() override;
  void ShowContextMenuAtPoint(
      gfx::Point point,
      std::unique_ptr<ui::MenuModel> menu_model) override;
  void ShowEditDialogForGroupAtPoint(gfx::Point point,
                                     gfx::Rect rect,
                                     tab_groups::TabGroupId group) override;
  TabStripUILayout GetLayout() override;
  SkColor GetColor(int id) const override;

  // views::View:
  int GetHeightForWidth(int w) const override;

  gfx::Size FlexRule(const views::View* view,
                     const views::SizeBounds& bounds) const;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(View* observed_view) override;
  void OnViewIsDeleting(View* observed_view) override;

  // views::AccessiblePaneView
  bool SetPaneFocusAndFocusDefault() override;

  BrowserView* const browser_view_;
  views::WebView* const web_view_;
  views::View* const top_container_;
  views::View* tab_contents_container_;
  views::View* tab_counter_ = nullptr;

#if defined(OS_WIN)
  // If the user interacts with Windows in a way that changes the width of the
  // window, close the top container. This is similar to the auto-close when the
  // user touches outside the tabstrip.
  //
  // TODO(dfried, davidbienvenu): we can remove this as soon as we move to the
  // more modern Windows drag-drop system, avoiding some of the weirdness around
  // starting drag-drop.
  int old_top_container_width_ = 0;
#endif  // defined(OS_WIN)

  base::Optional<float> current_drag_height_;

  // When opened, if currently open. Used to calculate metric for how
  // long the tab strip is kept open.
  base::Optional<base::TimeTicks> time_at_open_;

  // Used to keep the toolbar revealed while the tab strip is open.
  std::unique_ptr<ImmersiveRevealedLock> immersive_revealed_lock_;

  gfx::SlideAnimation animation_{this};

  std::unique_ptr<AutoCloser> auto_closer_;
  std::unique_ptr<DragToOpenHandler> drag_to_open_handler_;

  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  std::unique_ptr<ui::MenuModel> context_menu_model_;

  ScopedObserver<views::View, views::ViewObserver> view_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_WEBUI_TAB_STRIP_CONTAINER_VIEW_H_
