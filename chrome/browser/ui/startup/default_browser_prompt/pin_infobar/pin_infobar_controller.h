// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/shell_integration.h"
#include "components/infobars/core/infobar_manager.h"

class BrowserWindowInterface;

namespace infobars {
class ContentInfoBarManager;
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
  // Checks the following in order:
  // * that Chrome is the default browser
  // * that Chrome can be pinned to the taskbar
  // * that the infobar wasn't shown recently or the max number of times
  // If the above are true, the infobar is shown. Passes `done_callback` a
  // boolean indicating whether the infobar was shown.
  void MaybeShowInfoBar(base::OnceCallback<void(bool)> done_callback);

  // The browser that owns this `PinInfoBarController`.
  const raw_ptr<BrowserWindowInterface> browser_;

  // The infobar being shown, or `nullptr` if no infobar currently exists.
  raw_ptr<infobars::InfoBar> infobar_ = nullptr;

  // The manager of the infobar being shown, or `nullptr` if no infobar
  // currently exists.
  raw_ptr<infobars::ContentInfoBarManager> infobar_manager_ = nullptr;

  // Enables `OnBrowserClosed()` to be called.
  std::vector<base::CallbackListSubscription> browser_subscriptions_;

  // Must be the last member variable.
  base::WeakPtrFactory<PinInfoBarController> weak_factory_{this};
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_PIN_INFOBAR_PIN_INFOBAR_CONTROLLER_H_
