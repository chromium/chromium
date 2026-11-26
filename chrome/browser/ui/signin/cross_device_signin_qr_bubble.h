// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_H_
#define CHROME_BROWSER_UI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_H_

#include <memory>

#include "base/functional/callback.h"
#include "chrome/browser/signin/cross_device_signin_promo_manager.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace views {
class BubbleDialogDelegate;
class BubbleDialogModelHost;
}  // namespace views

DECLARE_ELEMENT_IDENTIFIER_VALUE(kCrossDeviceSigninQrBubbleWebViewElementId);

// Encapsulates anchor-specific behavior (anchor resolution, button/action-item
// state, deactivation/tab-switch closing, and cleanup) for the cross-device
// sign-in QR code bubble.
class CrossDeviceSigninQrBubbleAnchor {
 public:
  virtual ~CrossDeviceSigninQrBubbleAnchor() = default;

  // Resolves the bubble anchor and invokes `on_resolved` once ready
  // (synchronously, or asynchronously while popping out an overflowed avatar
  // button). Destroying this object cancels any pending resolution.
  virtual void ResolveAnchor(base::OnceClosure on_resolved) = 0;

  // Returns the anchor to pass to `views::BubbleDialogModelHost`, or a null
  // anchor if the anchor button is unavailable (e.g. in a popup window).
  virtual views::BubbleAnchor GetBubbleAnchor() const = 0;

  // Returns whether the bubble should close automatically on focus loss.
  virtual bool ShouldCloseOnDeactivate() const = 0;

  // Applies anchor-specific configuration to `bubble` (e.g. highlighted element
  // or explicit toolbar button state) after construction.
  virtual void ConfigureBubble(views::BubbleDialogModelHost& bubble,
                               views::BubbleAnchor anchor) {}

  // Attaches anchor-specific observers (e.g. originating tab deactivation or
  // detachment) that invoke `close_bubble` when the anchor is no longer valid.
  virtual void AttachToBubble(base::OnceClosure close_bubble) {}

  // Called right before showing the bubble widget after WebUI load/auto-resize.
  // Returns false if the bubble should be closed instead of shown (e.g. another
  // bubble claimed the anchor while loading).
  virtual bool OnBeforeShow() = 0;

  // Called synchronously when the widget starts closing, before deferred widget
  // destruction (`DeleteSoon()`).
  virtual void OnWidgetClosing(views::Widget::ClosedReason reason) {}

  // Releases any anchor-specific UI state and observers. Idempotent.
  virtual void ReleaseState() {}
};

// Result of `CreateCrossDeviceSigninQrBubble()`.
struct CrossDeviceSigninQrBubble {
  CrossDeviceSigninQrBubble(
      std::unique_ptr<views::BubbleDialogDelegate> delegate,
      base::OnceCallback<void(views::Widget::ClosedReason)> on_widget_closing);
  CrossDeviceSigninQrBubble(CrossDeviceSigninQrBubble&&) noexcept;
  CrossDeviceSigninQrBubble& operator=(CrossDeviceSigninQrBubble&&) noexcept;
  ~CrossDeviceSigninQrBubble();

  std::unique_ptr<views::BubbleDialogDelegate> delegate;
  // Must be run synchronously when the bubble's `views::Widget` starts closing,
  // before deferred widget destruction (`DeleteSoon()`), so anchor state is
  // released before a replacement bubble can claim it in the same task.
  base::OnceCallback<void(views::Widget::ClosedReason)> on_widget_closing;
};

// Creates the anchor delegate for `entry_point` (the avatar button for standard
// entry points, or an omnibox page action for post-save promos).
// Implemented in
// chrome/browser/ui/views/profiles/cross_device_signin_qr_bubble_views.cc
std::unique_ptr<CrossDeviceSigninQrBubbleAnchor>
CreateCrossDeviceSigninQrBubbleAnchor(
    BrowserWindowInterface* browser,
    CrossDeviceSigninPromoEntryPoint entry_point);

// Creates the delegate and synchronous widget-closing hook for the cross-device
// sign-in QR code bubble using a resolved `anchor`. If `anchor` resolves to a
// null `views::BubbleAnchor`, the bubble floats centered on `browser`'s window.
// Implemented in
// chrome/browser/ui/views/profiles/cross_device_signin_qr_bubble_views.cc
CrossDeviceSigninQrBubble CreateCrossDeviceSigninQrBubble(
    BrowserWindowInterface* browser,
    std::unique_ptr<CrossDeviceSigninQrBubbleAnchor> anchor,
    GURL qr_code_url,
    base::OnceClosure closing_callback,
    CrossDeviceSigninPromoEntryPoint entry_point);

#endif  // CHROME_BROWSER_UI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_H_
