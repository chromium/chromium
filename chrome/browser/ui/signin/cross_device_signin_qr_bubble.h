// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_H_
#define CHROME_BROWSER_UI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/signin/cross_device_signin_promo_manager.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace views {
class BubbleDialogDelegate;
}  // namespace views

DECLARE_ELEMENT_IDENTIFIER_VALUE(kCrossDeviceSigninQrBubbleWebViewElementId);

// Creates the delegate for the cross-device sign-in QR code bubble. If `anchor`
// is null, the bubble floats centered on `browser`'s window. `keep_visible` is
// held until the bubble closes so an overflowed avatar button stays visible.
// Implemented in
// chrome/browser/ui/views/profiles/cross_device_signin_qr_bubble_views.cc
std::unique_ptr<views::BubbleDialogDelegate> CreateCrossDeviceSigninQrBubble(
    BrowserWindowInterface* browser,
    views::BubbleAnchor anchor,
    base::ScopedClosureRunner keep_visible,
    GURL qr_code_url,
    base::OnceClosure closing_callback,
    CrossDeviceSigninPromoEntryPoint entry_point);

#endif  // CHROME_BROWSER_UI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_H_
