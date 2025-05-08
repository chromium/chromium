// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_INFOBAR_PDF_INFOBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_PDF_INFOBAR_PDF_INFOBAR_CONTROLLER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/shell_integration.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/web_contents_observer.h"

class BrowserWindowInterface;

namespace content {
class NavigationHandle;
}  // namespace content

namespace infobars {
class ContentInfoBarManager;
class InfoBar;
}  // namespace infobars

// Toggles the PDF infobar if Chrome isn't the default PDF viewer. This infobar
// offers to set Chrome as the default PDF viewer if it's not already.
class PdfInfoBarController : public infobars::InfoBarManager::Observer,
                             public content::WebContentsObserver {
 public:
  explicit PdfInfoBarController(BrowserWindowInterface* browser);
  ~PdfInfoBarController() override;

  // Enables the PDF infobar to show only if `default_browser_prompt_shown` is
  // false. If the PDF-infobar experiment is enabled and should be shown at
  // startup, shows the infobar for `startup_browser`.
  static void MaybeShowInfoBarAtStartup(
      base::WeakPtr<BrowserWindowInterface> startup_browser,
      bool default_browser_prompt_shown);

  // Callback passed to `BrowserWindowInterface::RegisterActiveTabDidChange()`.
  void OnActiveTabChanged(BrowserWindowInterface* browser);

  // Callback passed to `BrowserWindowInterface::RegisterBrowserDidClose()`.
  void OnBrowserClosed(BrowserWindowInterface* browser);

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  // Shows the PDF infobar if:
  // * `default_state` indicates that Chrome (including other channels) is not
  //   the default PDF viewer
  // * the PDF viewer is enabled in settings
  // * setting Chrome as default isn't forbidden by policy
  // * the infobar wasn't shown recently or the max number of times
  // * the default-browser prompt wasn't shown in this session
  // Exposed for testing.
  void MaybeShowInfoBarCallback(
      shell_integration::DefaultWebClientState default_state);

  static void SetDefaultBrowserPromptShownForTesting(
      bool default_browser_prompt_shown);

 private:
  // Asynchronously checks if Chrome is the default PDF viewer, and calls
  // `MaybeShowInfoBarCallback()` with the result.
  void MaybeShowInfoBar();

  // The browser that owns this `PdfInfoBarController`.
  const raw_ptr<BrowserWindowInterface> browser_;

  // The infobar being shown, or `nullptr` if no infobar currently exists.
  raw_ptr<infobars::InfoBar> infobar_ = nullptr;

  // The manager of the infobar being shown, or `nullptr` if no infobar
  // currently exists.
  raw_ptr<infobars::ContentInfoBarManager> infobar_manager_ = nullptr;

  // Enables `OnActiveTabChanged()` and `OnBrowserClosed()` to be called.
  std::vector<base::CallbackListSubscription> browser_subscriptions_;

  // Must be the last member variable.
  base::WeakPtrFactory<PdfInfoBarController> weak_factory_{this};

  // True if the default-browser prompt has been shown in this session, false
  // if it was not shown. Has no value if the default-browser prompt is still
  // deciding whether to appear.
  static std::optional<bool> default_browser_prompt_shown_;
};

#endif  // CHROME_BROWSER_UI_PDF_INFOBAR_PDF_INFOBAR_CONTROLLER_H_
