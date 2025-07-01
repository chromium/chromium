// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/shell_integration.h"
#include "components/infobars/core/infobar_manager.h"

class BrowserWindowInterface;

namespace infobars {
class InfoBar;
}  // namespace infobars

namespace default_browser {

// Toggles the pin-to-taskbar infobar if Chrome is the default browser but is
// not pinned to the taskbar. This infobar offers to pin Chrome to the taskbar.
class PinInfoBarController : public infobars::InfoBarManager::Observer {
 public:
  explicit PinInfoBarController(BrowserWindowInterface* browser);
  ~PinInfoBarController() override;

  // Callback passed to `BrowserWindowInterface::RegisterBrowserDidClose()`.
  void OnBrowserClosed(BrowserWindowInterface* browser);

  // InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  // Shows the pin-to-taskbar infobar on `browser` if `another_infobar_shown` is
  // false and it's appropriate to do so (see `MaybeShowInfoBar()` for
  // conditions). When done, passes `done_callback` a boolean indicating whether
  // the infobar was shown.
  static void MaybeShowInfoBarForBrowser(
      base::WeakPtr<BrowserWindowInterface> browser,
      base::OnceCallback<void(bool)> done_callback,
      bool another_infobar_shown);

  void OnIsDefaultBrowserResult(
      base::OnceCallback<void(bool)> done_callback,
      shell_integration::DefaultWebClientState default_state);

  void OnShouldOfferToPinResult(base::OnceCallback<void(bool)> done_callback,
                                bool should_offer_to_pin);

 private:
  // Asynchronously checks the following in sequence:
  // * that Chrome is the default browser
  // * that Chrome can be pinned to the taskbar
  // If the above are true, the infobar is shown. Passes `done_callback` a
  // boolean indicating whether the infobar was shown.
  void MaybeShowInfoBar(base::OnceCallback<void(bool)> done_callback);
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_CONTROLLER_H_
