// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_H_
#define CHROME_BROWSER_UI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_H_

#include "base/functional/callback_forward.h"
#include "ui/base/interaction/element_identifier.h"

class BrowserWindowInterface;

DECLARE_ELEMENT_IDENTIFIER_VALUE(kCrossDeviceSigninQrBubbleWebViewElementId);

// Creates the delegate for the cross-device sign-in QR code bubble.
// Implemented in
// chrome/browser/ui/views/profiles/cross_device_signin_qr_bubble_views.cc
namespace views {
class BubbleDialogDelegate;
}

std::unique_ptr<views::BubbleDialogDelegate> CreateCrossDeviceSigninQrBubble(
    BrowserWindowInterface* browser,
    base::OnceClosure closing_callback);

#endif  // CHROME_BROWSER_UI_SIGNIN_CROSS_DEVICE_SIGNIN_QR_BUBBLE_H_
