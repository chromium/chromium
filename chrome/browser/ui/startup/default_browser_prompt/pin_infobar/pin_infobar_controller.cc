// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_controller.h"

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/infobars/core/infobar.h"

namespace default_browser {

PinInfoBarController::PinInfoBarController(BrowserWindowInterface* browser) {
  NOTIMPLEMENTED();
}

PinInfoBarController::~PinInfoBarController() = default;

void PinInfoBarController::OnBrowserClosed(BrowserWindowInterface* browser) {
  NOTIMPLEMENTED();
}

void PinInfoBarController::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                            bool animate) {
  NOTIMPLEMENTED();
}

// static
void PinInfoBarController::MaybeShowInfoBarForBrowser(
    base::WeakPtr<BrowserWindowInterface> browser,
    base::OnceCallback<void(bool)> done_callback,
    bool another_infobar_shown) {
  NOTIMPLEMENTED();
}

void PinInfoBarController::MaybeShowInfoBar(
    base::OnceCallback<void(bool)> done_callback) {
  NOTIMPLEMENTED();
}

void PinInfoBarController::OnIsDefaultBrowserResult(
    base::OnceCallback<void(bool)> done_callback,
    shell_integration::DefaultWebClientState default_state) {
  NOTIMPLEMENTED();
}

void PinInfoBarController::OnShouldOfferToPinResult(
    base::OnceCallback<void(bool)> done_callback,
    bool should_offer_to_pin) {
  NOTIMPLEMENTED();
}

}  // namespace default_browser
