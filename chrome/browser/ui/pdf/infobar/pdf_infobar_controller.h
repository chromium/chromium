// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_INFOBAR_PDF_INFOBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_PDF_INFOBAR_PDF_INFOBAR_CONTROLLER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/shell_integration.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/installer/util/shell_util.h"
#endif

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace infobars {
class ContentInfoBarManager;
class InfoBar;
}  // namespace infobars

namespace pdf::infobar {

#if BUILDFLAG(IS_WIN)
// Potential results of showing the Windows settings UI to set Chrome as the
// default PDF viewer.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PdfInfoBarSettingsResult)
enum class PdfInfoBarSettingsResult {
  // The settings UI was purposely not shown. This is unexpected, and indicates
  // that the infobar was shown when setting Chrome as default was inappropriate
  // (e.g., when Chrome is already the default).
  kNotShown = 0,
  // The settings UI was shown and Chrome was set as default PDF viewer.
  kSuccess = 1,
  // The settings UI was shown, but Chrome wasn't set as default.
  kSuccessNoChange = 2,
  // The fallback settings UI was shown and Chrome was set as default.
  kFallback = 3,
  // The fallback settings UI was shown, but Chrome wasn't set as default.
  kFallbackNoChange = 4,
  // The settings UI failed to open.
  kError = 5,
  kMaxValue = kError
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/pdf/enums.xml:PdfInfoBarSettingsResult)
#endif  // BUILDFLAG(IS_WIN)

// Potential user interactions with the PDF infobar.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(PdfInfoBarUserInteraction)
enum class PdfInfoBarUserInteraction {
  kAccepted = 0,
  kDismissed = 1,
  kIgnored = 2,
  kMaxValue = kIgnored,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/pdf/enums.xml:PDFInfoBarUserInteraction)

// Toggles the PDF infobar if Chrome isn't the default PDF viewer. This infobar
// offers to set Chrome as the default PDF viewer if it's not already.
class PdfInfoBarController : public infobars::InfoBarManager::Observer {
 public:
  DECLARE_USER_DATA(PdfInfoBarController);
  explicit PdfInfoBarController(BrowserWindowInterface* browser);
  ~PdfInfoBarController() override;

#if BUILDFLAG(IS_WIN)
  static void RecordSettingsResult(ShellUtil::ShowSystemUIResult result);
#endif

  // Records a user interaction with the PDF infobar to a histogram.
  static void RecordUserInteractionHistogram(
      PdfInfoBarUserInteraction interaction);

  // Sets Chrome as the default PDF handler on Mac/Windows.
  static void SetAsDefaultPdfHandler(content::WebContents* web_contents);

  static PdfInfoBarController* From(BrowserWindowInterface* window);

  // Enables the PDF infobar to show only if `higher_priority_infobar_shown` is
  // false. If the PDF-infobar experiment is enabled, shows the infobar for
  // `startup_browser`.
  static void MaybeShowInfoBarAtStartup(
      base::WeakPtr<BrowserWindowInterface> startup_browser,
      bool higher_priority_infobar_shown);

  // Callback passed to `BrowserWindowInterface::RegisterBrowserDidClose()`.
  void OnBrowserClosed(BrowserWindowInterface* browser);

  // InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  void set_action_taken(bool action_taken) { action_taken_ = action_taken; }

  // Shows the PDF infobar if:
  // * `default_state` indicates that Chrome (including other channels) is not
  //   the default PDF viewer
  // * the PDF viewer is enabled in settings
  // * setting Chrome as default isn't forbidden by policy
  // * the infobar wasn't shown recently or the max number of times
  // * a higher priority infobar wasn't shown in this session
  // Exposed for testing.
  void MaybeShowInfoBarCallback(
      shell_integration::DefaultWebClientState default_state);

  static void SetHigherPriorityInfoBarShownForTesting(
      bool higher_priority_infobar_shown);

 private:
  // Registers the `InfoBarSpec` for `PDF_INFOBAR_DELEGATE` under the centralized
  // `BrowserInfoBarManager` framework.
  static void RegisterInfoBarSpec();

#if BUILDFLAG(IS_WIN)
  // Records the result of showing the Windows settings UI to a histogram.
  static void RecordSettingsResultHistogram(PdfInfoBarSettingsResult result);
#endif

  // Asynchronously checks if Chrome is the default PDF viewer, and calls
  // `MaybeShowInfoBarCallback()` with the result.
  void MaybeShowInfoBar();

  // The browser that owns this `PdfInfoBarController`.
  const raw_ptr<BrowserWindowInterface> browser_;

  // The infobar being shown, or `nullptr` if no infobar currently exists.
  raw_ptr<infobars::InfoBar> infobar_ = nullptr;

  // Scoped observer that facilitates observing an InfoBarManager.
  base::ScopedObservation<infobars::InfoBarManager,
                          infobars::InfoBarManager::Observer>
      infobar_scoped_observation_{this};

  // Enables `OnBrowserClosed()` to be called.
  std::vector<base::CallbackListSubscription> browser_subscriptions_;

  // True if another infobar that takes priority over this one has already been
  // shown in this session. Has no value if higher priority infobars are still
  // deciding whether to appear.
  static std::optional<bool> higher_priority_infobar_shown_;

  ui::ScopedUnownedUserData<PdfInfoBarController> scoped_unowned_user_data_;

  // Tracks whether user action has been taken on the infobar.
  bool action_taken_ = false;

  // Must be the last member variable.
  base::WeakPtrFactory<PdfInfoBarController> weak_factory_{this};
};

}  // namespace pdf::infobar

#endif  // CHROME_BROWSER_UI_PDF_INFOBAR_PDF_INFOBAR_CONTROLLER_H_
