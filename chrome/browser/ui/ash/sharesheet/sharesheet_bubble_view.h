// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_BUBBLE_VIEW_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

namespace views {
class TableLayoutView;
class Separator;
}  // namespace views

namespace sharesheet {
class SharesheetServiceDelegator;
}

namespace ash {
namespace sharesheet {

class SharesheetHeaderView;
class SharesheetExpandButton;

class SharesheetBubbleView : public views::BubbleDialogDelegateView,
                             public TabletModeObserver {
 public:
  METADATA_HEADER(SharesheetBubbleView);
  using TargetInfo = ::sharesheet::TargetInfo;

  SharesheetBubbleView(gfx::NativeWindow native_window,
                       ::sharesheet::SharesheetServiceDelegator* delegate);
  SharesheetBubbleView(const SharesheetBubbleView&) = delete;
  SharesheetBubbleView& operator=(const SharesheetBubbleView&) = delete;
  ~SharesheetBubbleView() override;

  // |delivered_callback| is run to inform the caller whether something failed,
  // or the intent has been delivered to a target selected by the user.
  // |close_callback| is run to inform the caller when the bubble is closed.
  void ShowBubble(std::vector<TargetInfo> targets,
                  apps::IntentPtr intent,
                  ::sharesheet::DeliveredCallback delivered_callback,
                  ::sharesheet::CloseCallback close_callback);
  void ShowNearbyShareBubbleForArc(
      apps::IntentPtr intent,
      ::sharesheet::DeliveredCallback delivered_callback,
      ::sharesheet::CloseCallback close_callback);
  void ShowActionView();
  void ResizeBubble(const int& width, const int& height);
  void CloseBubble(views::Widget::ClosedReason reason);

  // --- Added for debugging purposes. Remove after bug fixed.

  ASH_EXPORT void PerformLoggingAndChecks(gfx::NativeWindow native_window);
  ASH_EXPORT void SetUpDialog();
  ASH_EXPORT void SetUpParentWindow(gfx::NativeWindow native_window);

  // --- End of functions added for debugging.

 private:
  class SharesheetParentWidgetObserver;

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // ui::EventHandler:
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // views::WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

  void CreateBubble();
  std::unique_ptr<views::View> MakeScrollableTargetView(
      std::vector<TargetInfo> targets);
  void PopulateLayoutsWithTargets(std::vector<TargetInfo> targets,
                                  views::TableLayoutView* default_view,
                                  views::TableLayoutView* expanded_view);
  void ExpandButtonPressed();
  void AnimateToExpandedState();
  void TargetButtonPressed(TargetInfo target);
  void UpdateAnchorPosition();
  void SetToDefaultBubbleSizing();
  void ShowWidgetWithAnimateFadeIn();
  void CloseWidgetWithAnimateFadeOut(views::Widget::ClosedReason closed_reason);
  void CloseWidgetWithReason(views::Widget::ClosedReason closed_reason);

  // Owns this class.
  raw_ptr<::sharesheet::SharesheetServiceDelegator, ExperimentalAsh> delegator_;
  std::u16string active_target_;
  apps::IntentPtr intent_;
  ::sharesheet::DeliveredCallback delivered_callback_;
  ::sharesheet::CloseCallback close_callback_;

  int width_ = 0;
  int height_ = 0;
  bool show_expanded_view_ = false;
  bool is_bubble_closing_ = false;
  bool close_on_deactivate_ = true;
  bool escape_pressed_ = false;

  size_t keyboard_highlighted_target_ = 0;

  raw_ptr<views::View, ExperimentalAsh> main_view_ = nullptr;
  raw_ptr<SharesheetHeaderView, ExperimentalAsh> header_view_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> body_view_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> footer_view_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> default_view_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> expanded_view_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> share_action_view_ = nullptr;
  // Separator that appears between the |header_view_| and the |body_view|.
  raw_ptr<views::Separator, ExperimentalAsh> header_body_separator_ = nullptr;
  // Separator that appears between the |body_view| and the |footer_view_|.
  raw_ptr<views::Separator, ExperimentalAsh> body_footer_separator_ = nullptr;
  // Separator between the default_view and the expanded_view.
  raw_ptr<views::Separator, ExperimentalAsh> expanded_view_separator_ = nullptr;
  raw_ptr<views::View, ExperimentalAsh> parent_view_ = nullptr;
  raw_ptr<SharesheetExpandButton, ExperimentalAsh> expand_button_ = nullptr;

  std::unique_ptr<SharesheetParentWidgetObserver> parent_widget_observer_;
  base::ScopedObservation<TabletMode, TabletModeObserver>
      tablet_mode_observation_{this};
};

}  // namespace sharesheet
}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_SHARESHEET_SHARESHEET_BUBBLE_VIEW_H_
