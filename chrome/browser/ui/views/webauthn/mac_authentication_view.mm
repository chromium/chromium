// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/mac_authentication_view.h"

#import <LocalAuthentication/LocalAuthentication.h>
#import <LocalAuthenticationEmbeddedUI/LocalAuthenticationEmbeddedUI.h>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/timer/timer.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/scoped_lacontext.h"
#include "device/fido/mac/util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

// kWidth is the width (and height, since it's square) of the NSView.
constexpr int kWidth = 64;

// The seconds it takes for the Touch ID animation to finish when the challenge
// fails.
constexpr float kErrorAnimationLength = 1;

// The seconds it takes for the Touch ID animation to finish when the challenge
// succeeds. This is trimmed down so that we can overlap the enclave operation
// with the animation.
constexpr float kSuccessAnimationLength = 1.6;

struct API_AVAILABLE(macos(12.0)) MacAuthenticationView::ObjCStorage {
  LAContext* __strong context;
  LAAuthenticationView* __strong auth_view;
};

MacAuthenticationView::MacAuthenticationView(Callback callback,
                                             std::u16string touch_id_reason)
    : callback_(std::move(callback)),
      storage_(std::make_unique<ObjCStorage>()),
      touch_id_reason_(std::move(touch_id_reason)) {
  storage_->context = [[LAContext alloc] init];
  storage_->auth_view =
      [[LAAuthenticationView alloc] initWithContext:storage_->context];

  // The size of the NSView is set as constraints on itself. But none of the
  // standard sizes match the size used in Safari. Thus we erase them set our
  // own.
  [storage_->auth_view removeConstraints:storage_->auth_view.constraints];
  [storage_->auth_view.widthAnchor constraintEqualToConstant:kWidth].active =
      YES;
  [storage_->auth_view.heightAnchor constraintEqualToConstant:kWidth].active =
      YES;
  return;
}

MacAuthenticationView::~MacAuthenticationView() = default;

gfx::Size MacAuthenticationView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kWidth, kWidth);
}

void MacAuthenticationView::AddedToWidget() {
  // An `LAAuthenticationView` is an NSView, but Chromium uses the Views
  // framework for its UI, which just renders pixels onto a backing surface
  // (called a "widget"). Thus any NSViews have to be composited onto what
  // Views renders and positioned in the right place.
  //
  // Here the `LAAuthenticationView` is added to the NSWindow for this View, and
  // positioned above it so that it's painted on top. We assume that the first
  // existing NSView in the NSWindow is the `ViewsCompositorSuperview` that
  // Views renders onto.
  NSWindow* window = GetWidget()->GetNativeWindow().GetNativeNSWindow();
  [window.contentView addSubview:storage_->auth_view
                      positioned:NSWindowAbove
                      relativeTo:window.contentView.subviews[0]];
}

void MacAuthenticationView::RemovedFromWidget() {
  [storage_->auth_view removeFromSuperview];
}

void MacAuthenticationView::Layout(PassKey) {
  gfx::Rect bounds = this->bounds();
  // The bounds of this View include the offset from its parent View.
  // However, `ConvertPointToWidget` (below) already takes this offset into
  // account so it must be zeroed here to avoid it being applied twice.
  bounds.set_x(0);
  bounds.set_y(0);
  // The LAAuthenticationView is centered within this View.
  bounds.ClampToCenteredSize(gfx::Size(kWidth, kWidth));
  // The Widget represents the backing surface onto which Views renders. We need
  // to know the position of the LAAuthenticationView in relation to the widget
  // because NSView doesn't know anything about Chromium's Views tree.
  gfx::Point point = bounds.origin();
  View::ConvertPointToWidget(this, &point);
  // Views puts (0, 0) at the top left and positive-y is downwards. But NSView
  // puts (0,0) at the bottom-left and positive-y is up. So we need to know the
  // height of the Widget to adjust things.
  gfx::Rect widget_rect = this->GetWidget()->GetClientAreaBoundsInScreen();
  // Place the LAAuthenticationView after adjusting for the different coordinate
  // system.
  storage_->auth_view.frame =
      NSMakeRect(point.x(), widget_rect.height() - point.y() - bounds.height(),
                 kWidth, kWidth);
}

void MacAuthenticationView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);
  if (GetVisible() && !evaluation_requested_) {
    InvalidateLayout();
    storage_->auth_view.hidden = false;
    evaluation_requested_ = true;
    if (!device::fido::mac::DeviceHasBiometricsAvailable()) {
      return;
    }
    __block auto internal_callback =
        base::BindOnce(&MacAuthenticationView::OnAuthenticationComplete,
                       weak_factory_.GetWeakPtr());
    [storage_->context
         evaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
        localizedReason:base::SysUTF16ToNSString(touch_id_reason_)
                  reply:^(BOOL success, NSError* error) {
                    if (error) {
                      FIDO_LOG(ERROR) << "Touch ID failed with error: "
                                      << error.localizedDescription.UTF8String;
                    }
                    content::BrowserThread::GetTaskRunnerForThread(
                        content::BrowserThread::UI)
                        ->PostTask(FROM_HERE,
                                   base::BindOnce(std::move(internal_callback),
                                                  success));
                  }];
  }
}

void MacAuthenticationView::VisibilityChanged(views::View* from,
                                              bool is_visible) {
  views::View::VisibilityChanged(from, is_visible);
  storage_->auth_view.hidden = !is_visible;
}

void MacAuthenticationView::OnAuthenticationComplete(bool success) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // It takes a while for the Touch ID animation to finish after success is
  // reported. Avoid jank by waiting for the animation to finish before
  // notifying the client.
  touch_id_animation_timer_.Start(
      FROM_HERE,
      base::Seconds(success ? kSuccessAnimationLength : kErrorAnimationLength),
      base::BindOnce(&MacAuthenticationView::OnTouchIDAnimationComplete,
                     base::Unretained(this), success));
}

void MacAuthenticationView::OnTouchIDAnimationComplete(bool success) {
  std::optional<crypto::ScopedLAContext> lacontext;
  if (success) {
    lacontext.emplace(storage_->context);
  }
  storage_->context = nil;
  std::move(callback_).Run(std::move(lacontext));
}

BEGIN_METADATA(MacAuthenticationView)
END_METADATA
