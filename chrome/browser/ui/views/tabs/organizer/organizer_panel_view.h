// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_VIEW_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_controls_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class ActionViewController;
class EventMonitor;
class ViewShadow;
}  // namespace views

class BrowserWindowInterface;
class OrganizerPanelStateController;

// Parent view of the Organizer Panel - holds together the views
// hierarchy including controls and close action.
class OrganizerPanelView : public views::View,
                           public views::FocusChangeListener,
                           public views::FocusTraversable,
                           public gfx::AnimationDelegate {
  METADATA_HEADER(OrganizerPanelView, views::View)

 public:
  OrganizerPanelView(BrowserWindowInterface* browser,
                     actions::ActionItem* root_action_item,
                     OrganizerPanelStateController* state_controller);
  OrganizerPanelView(const OrganizerPanelView&) = delete;
  OrganizerPanelView& operator=(const OrganizerPanelView&) = delete;
  ~OrganizerPanelView() override;

  bool IsPositionInWindowCaption(const gfx::Point& point);

  // Called when the organizer panel state changes. Updates the visibility and
  // tooltips to match the new state.
  void OnOrganizerPanelStateChanged(
      OrganizerPanelStateController* state_controller);

  double GetResizeAnimationValue() const;

  // Set the width of the panel when it is fully expanded.
  void SetTargetWidth(int target_width);

  // Set whether the panel should appear elevated with rounded borders.
  void SetIsElevated(bool elevated);

  // Whether the panel appears elevated with rounded borders.
  bool is_elevated() { return elevated_; }

  // views::View:
  void AddedToWidget() override;
  void Layout(PassKey) override;
  void RemovedFromWidget() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  views::FocusTraversable* GetPaneFocusTraversable() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // views::FocusTraversable:
  views::FocusSearch* GetFocusSearch() override;
  views::FocusTraversable* GetFocusTraversableParent() override;
  views::View* GetFocusTraversableParentView() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  views::View* content_container_for_testing() { return content_container_; }
  OrganizerPanelControlsView* controls_view_for_testing() {
    return controls_view_;
  }
  views::View* web_view_for_testing();

  void set_on_close_animation_ended_callback_for_testing(
      base::OnceClosure on_close_animation_ended_callback) {
    on_close_animation_ended_callback_ =
        std::move(on_close_animation_ended_callback);
  }

  static void disable_animations_for_testing();

 private:
  // Detects if mouse presses occur outside of the panel.
  class MouseEventHandler : public ui::EventObserver {
   public:
    explicit MouseEventHandler(OrganizerPanelView* owning_view);
    MouseEventHandler(const MouseEventHandler&) = delete;
    MouseEventHandler& operator=(const MouseEventHandler&) = delete;
    ~MouseEventHandler() override;

    void OnEvent(const ui::Event& event) override;

   private:
    raw_ptr<OrganizerPanelView> owning_view_ = nullptr;
  };

  void ClosePanel(bool caused_by_focus_lost = false);

  const raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;

  raw_ptr<views::View> content_container_ = nullptr;
  raw_ptr<OrganizerPanelControlsView> controls_view_ = nullptr;
  raw_ptr<views::View> web_view_ = nullptr;

  std::unique_ptr<views::ViewShadow> content_shadow_;

  std::unique_ptr<views::ActionViewController> action_view_controller_;
  const raw_ptr<OrganizerPanelStateController> state_controller_ = nullptr;

  // Animation when opening and closing the panel.
  gfx::SlideAnimation resize_animation_;
  base::OnceClosure on_close_animation_ended_callback_;

  // Handle mouse presses outside the panel.
  MouseEventHandler mouse_event_handler_{this};
  std::unique_ptr<views::EventMonitor> event_monitor_;

  std::unique_ptr<views::FocusSearch> focus_search_;

  // The target width of the panel when expanded. Used when vertical tabs is
  // enabled since the panel width needs to match when expanded.
  int target_width_ = organizer_panel::kOrganizerPanelMinWidth;

  // Whether the panel should show with an elevation shadow and rounded borders.
  // The default appearance of the panel is elevated, but this must be false
  // for the SetIsElevated call in the constructor to be effective.
  bool elevated_ = false;

  // Prevents attempting to (un)observe the focus manager more than once if
  // OnOrganizerPanelStateChanged is called twice with the same visibility
  // value.
  bool observing_focus_manager_ = false;

  // Records the last time the panel was opened. Used for recording how long the
  // panel was open.
  base::TimeTicks last_opened_time_;

  // Tracks the last focused view before opening the panel, so focus can be
  // restored when the panel is closed.
  views::ViewTracker last_focused_view_before_opening_;

  base::WeakPtrFactory<OrganizerPanelView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_VIEW_H_
