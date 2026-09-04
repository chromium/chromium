// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_TRAY_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_TRAY_VIEW_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_tracker.h"

class BrowserWindowInterface;
class OrganizerPanelControlsView;
class OrganizerPanelStateController;
class ShadowFrameView;

// Provides the visuals for the UI that slides out from the side of the browser
// hosting the organizer panel when the panel is not hosted in some other UI
// (such as the vertical tab strip).
class OrganizerTrayView : public views::FlexLayoutView,
                          public views::FocusTraversable {
  METADATA_HEADER(OrganizerTrayView, views::View)
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTrayElementId);
  static constexpr base::TimeDelta kPanelShowAnimationDuration =
      base::Milliseconds(250);
  static constexpr base::TimeDelta kPanelHideAnimationDuration =
      base::Milliseconds(200);

  explicit OrganizerTrayView(BrowserWindowInterface& browser);
  ~OrganizerTrayView() override;

  // Sets the area (if any) occupied by the caption buttons at the top leading
  // corner of the tray.
  void SetTopLeadingExclusion(const gfx::Size& top_leading_exclusion);

  // Sets the target width for the panel.
  void SetTargetWidth(int target_width);
  int target_width() const { return target_width_; }

  // Sets or takes the panel view.
  void SetPanelView(std::unique_ptr<views::View> panel_view);
  std::unique_ptr<views::View> TakePanelView();
  bool has_panel_view() const { return panel_view_ != nullptr; }

  // Used to enable dragging.
  bool IsPositionInWindowCaption(const gfx::Point& point);

  // ----------------
  // To be removed.

  DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(kOpenAnimationComplete);
  DECLARE_CLASS_CUSTOM_ELEMENT_EVENT_TYPE(kCloseAnimationComplete);

  // Used by layout.
  double GetAnimationValue() const;
  void SetAnimationValueForTesting(double value);

  // Set whether the panel should appear elevated with rounded borders.
  void SetIsElevated(bool elevated);

  // Whether the panel appears elevated with rounded borders.
  bool is_elevated() { return elevated_; }

  // ----------------

 protected:
  // views::View:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void AddedToWidget() override;
  void VisibilityChanged(views::View* from, bool visible) override;
  views::FocusTraversable* GetPaneFocusTraversable() override;
  void Layout(PassKey) override;

  // views::FocusTraversable:
  views::FocusSearch* GetFocusSearch() override;
  views::FocusTraversable* GetFocusTraversableParent() override;
  views::View* GetFocusTraversableParentView() override;

 private:
  class EventObserver;

  void OnOrganizerPanelStateChanged(
      OrganizerPanelStateController* state_controller);

  void ClosePanel();

  const raw_ref<BrowserWindowInterface> browser_;
  const base::CallbackListSubscription controller_state_subscription_;
  views::FocusSearch focus_search_;
  raw_ptr<OrganizerPanelControlsView> controls_view_ = nullptr;
  raw_ptr<ShadowFrameView> shadow_frame_ = nullptr;
  std::unique_ptr<EventObserver> event_observer_;
  views::ViewTracker last_focused_view_before_opening_;
  gfx::Size top_leading_exclusion_;
  int target_width_ = organizer_panel::kOrganizerPanelMinWidth;
  raw_ptr<views::View> panel_view_ = nullptr;

  // ----------------
  // To be removed.

  class Animator;
  const std::unique_ptr<Animator> animator_;
  bool elevated_ = true;

  // ----------------
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_TRAY_VIEW_H_
