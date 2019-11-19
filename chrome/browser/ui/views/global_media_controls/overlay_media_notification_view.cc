// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_media_controls/overlay_media_notification_view.h"

#include "chrome/browser/ui/global_media_controls/overlay_media_notifications_manager.h"
#include "chrome/browser/ui/views/global_media_controls/media_notification_container_impl_view.h"
#include "ui/base/hit_test.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/non_client_view.h"

namespace {

// The pixel height of the draggable area on the overlay notification.
constexpr int kDraggingBoundsHeight = 20;

class OverlayMediaNotificationFrameView : public views::NonClientFrameView {
 public:
  OverlayMediaNotificationFrameView() = default;
  OverlayMediaNotificationFrameView(const OverlayMediaNotificationFrameView&) =
      delete;
  OverlayMediaNotificationFrameView& operator=(
      const OverlayMediaNotificationFrameView&) = delete;
  ~OverlayMediaNotificationFrameView() override = default;

  // views::NonClientFrameView implementation.
  gfx::Rect GetBoundsForClientView() const override { return bounds(); }
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override {
    return bounds();
  }
  int NonClientHitTest(const gfx::Point& point) override {
    if (!bounds().Contains(point))
      return HTNOWHERE;

    // TODO(steimel): This should be smarter, but we need to figure out how we
    // want to handle dragging vs click-to-go-to-tab.
    if (GetDraggingBounds().Contains(point))
      return HTCAPTION;

    return HTCLIENT;
  }
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

  // Returns the bounds in which clicking can drag the window.
  gfx::Rect GetDraggingBounds() {
    gfx::Rect drag_bounds(bounds());
    drag_bounds.set_height(kDraggingBoundsHeight);
    return drag_bounds;
  }
};

class OverlayMediaNotificationWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit OverlayMediaNotificationWidgetDelegate(
      OverlayMediaNotificationView* widget)
      : widget_(widget) {
    DCHECK(widget_);
  }
  OverlayMediaNotificationWidgetDelegate(
      const OverlayMediaNotificationWidgetDelegate&) = delete;
  OverlayMediaNotificationWidgetDelegate& operator=(
      const OverlayMediaNotificationWidgetDelegate&) = delete;
  ~OverlayMediaNotificationWidgetDelegate() override = default;

  // views::WidgetDelegate implementation.
  bool ShouldShowWindowTitle() const override { return false; }
  views::Widget* GetWidget() override { return widget_; }
  const views::Widget* GetWidget() const override { return widget_; }
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override {
    return new OverlayMediaNotificationFrameView();
  }

 private:
  // Owns OverlayMediaNotificationWidgetDelegate.
  OverlayMediaNotificationView* widget_;
};

}  // anonymous namespace

OverlayMediaNotificationView::OverlayMediaNotificationView(
    const std::string& id,
    std::unique_ptr<MediaNotificationContainerImplView> notification,
    gfx::Rect bounds)
    : id_(id), notification_(notification.get()) {
  DCHECK(notification_);

  // Set up the notification to be ready to show in an overlay.
  notification_->PopOut();

  InitParams params(InitParams::TYPE_WINDOW);
  params.ownership = InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = bounds;
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.visible_on_all_workspaces = true;
  params.remove_standard_frame = true;
  params.name = "OverlayMediaNotificationView";
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.delegate = new OverlayMediaNotificationWidgetDelegate(this);
  Init(std::move(params));
  GetContentsView()->AddChildView(std::move(notification));
}

OverlayMediaNotificationView::~OverlayMediaNotificationView() = default;

void OverlayMediaNotificationView::SetManager(
    OverlayMediaNotificationsManager* manager) {
  manager_ = manager;
}

void OverlayMediaNotificationView::ShowNotification() {
  // |SetManager()| should be called before showing the notification to ensure
  // that we don't close before it's set.
  DCHECK(manager_);
  Show();
}

void OverlayMediaNotificationView::CloseNotification() {
  CloseWithReason(ClosedReason::kUnspecified);
}

void OverlayMediaNotificationView::OnNativeWidgetDestroyed() {
  DCHECK(manager_);
  manager_->OnOverlayNotificationClosed(id_);

  views::Widget::OnNativeWidgetDestroyed();
}
