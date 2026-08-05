// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_controller.h"

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/strings/cstring_view.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/infobars/infobar_spec.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_delegate.h"
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_prefs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/common/buildflags.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif  // #if BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/startup/default_browser_prompt/pin_infobar/pin_infobar_mac_util.h"
#endif  // BUILDFLAG(IS_MAC)

#include "base/containers/span.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace default_browser {

namespace {

void RecordUserInteractionHistogram(PinInfoBarUserInteraction interaction) {
  base::UmaHistogramEnumeration("DefaultBrowser.PinInfoBar.UserInteraction",
                                interaction);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
struct ExperimentalString {
  int message_id;
  int button_id;
};

const ExperimentalString kExperimentalStrings[] = {
    {0, 0},  // Version 0 (Standard)
#if BUILDFLAG(IS_WIN)
    {IDS_PIN_INFOBAR_EXPERIMENTAL_MESSAGE_1,
     IDS_PIN_INFOBAR_EXPERIMENTAL_BUTTON_1},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_MESSAGE_2,
     IDS_PIN_INFOBAR_EXPERIMENTAL_BUTTON_2},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_MESSAGE_3,
     IDS_PIN_INFOBAR_EXPERIMENTAL_BUTTON_3},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_MESSAGE_4,
     IDS_PIN_INFOBAR_EXPERIMENTAL_BUTTON_4},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_MESSAGE_5,
     IDS_PIN_INFOBAR_EXPERIMENTAL_BUTTON_5},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_MESSAGE_6,
     IDS_PIN_INFOBAR_EXPERIMENTAL_BUTTON_6},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_MESSAGE_7,
     IDS_PIN_INFOBAR_EXPERIMENTAL_BUTTON_7},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_MESSAGE_8,
     IDS_PIN_INFOBAR_EXPERIMENTAL_BUTTON_8},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_MESSAGE_9,
     IDS_PIN_INFOBAR_EXPERIMENTAL_BUTTON_9},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_MESSAGE_10,
     IDS_PIN_INFOBAR_EXPERIMENTAL_BUTTON_10},
#elif BUILDFLAG(IS_MAC)
    {IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_MESSAGE_1,
     IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_BUTTON_1},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_MESSAGE_2,
     IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_BUTTON_2},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_MESSAGE_3,
     IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_BUTTON_3},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_MESSAGE_4,
     IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_BUTTON_4},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_MESSAGE_5,
     IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_BUTTON_5},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_MESSAGE_6,
     IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_BUTTON_6},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_MESSAGE_7,
     IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_BUTTON_7},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_MESSAGE_8,
     IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_BUTTON_8},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_MESSAGE_9,
     IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_BUTTON_9},
    {IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_MESSAGE_10,
     IDS_PIN_INFOBAR_EXPERIMENTAL_DOCK_BUTTON_10},
#endif
};
#endif

}  // namespace

// static
std::u16string PinInfoBarController::GetMessageText() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kSeparateDefaultAndPinPrompt)) {
    const int version =
        features::kSeparateDefaultAndPinPromptMessageVersion.Get();
    auto experimental_strings = base::span(kExperimentalStrings);
    if (version >= 1 &&
        static_cast<size_t>(version) < experimental_strings.size()) {
      return l10n_util::GetStringUTF16(
          experimental_strings[version].message_id);
    }
  }
#endif

#if BUILDFLAG(IS_WIN)
  return l10n_util::GetStringUTF16(IDS_PIN_INFOBAR_TEXT);
#elif BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(IDS_PIN_INFOBAR_DOCK_TEXT);
#endif
}

// static
std::u16string PinInfoBarController::GetButtonLabel() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kSeparateDefaultAndPinPrompt)) {
    const int version =
        features::kSeparateDefaultAndPinPromptMessageVersion.Get();
    auto experimental_strings = base::span(kExperimentalStrings);
    if (version >= 1 &&
        static_cast<size_t>(version) < experimental_strings.size()) {
      return l10n_util::GetStringUTF16(experimental_strings[version].button_id);
    }
  }
#endif

#if BUILDFLAG(IS_WIN)
  return l10n_util::GetStringUTF16(IDS_PIN_INFOBAR_BUTTON);
#elif BUILDFLAG(IS_MAC)
  return l10n_util::GetStringUTF16(IDS_PIN_INFOBAR_DOCK_BUTTON);
#endif
}

// static
void PinInfoBarController::OnAccept(content::WebContents* /*web_contents*/) {
  RecordUserInteractionHistogram(PinInfoBarUserInteraction::kAccepted);
#if BUILDFLAG(IS_WIN)
  browser_util::PinAppToTaskbar(
      ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
      browser_util::PinAppToTaskbarChannel::kPinToTaskbarInfoBar,
      base::DoNothing());
#elif BUILDFLAG(IS_MAC)
  PinChromeToDock();
#endif
}

// static
void PinInfoBarController::OnDismiss(content::WebContents* /*web_contents*/) {
  RecordUserInteractionHistogram(PinInfoBarUserInteraction::kDismissed);
}

DEFINE_USER_DATA(PinInfoBarController);

PinInfoBarController::PinInfoBarController(BrowserWindowInterface* browser)
    : browser_(browser),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {
  browser_subscriptions_.push_back(
      browser_->RegisterBrowserDidClose(base::BindRepeating(
          &PinInfoBarController::OnBrowserClosed, base::Unretained(this))));
}

PinInfoBarController::~PinInfoBarController() {
  if (infobar_manager_) {
    infobar_manager_->RemoveObserver(this);
  }
}

void PinInfoBarController::OnBrowserClosed(BrowserWindowInterface* browser) {
  if (infobar_) {
    infobar_manager_->RemoveInfoBar(infobar_);
  }
}

void PinInfoBarController::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                            bool animate) {
  if (infobar_ != infobar) {
    return;
  }
  infobar_ = nullptr;
  infobar_manager_->RemoveObserver(this);
  infobar_manager_ = nullptr;
}

void PinInfoBarController::OnManagerWillBeDestroyed(
    infobars::InfoBarManager* manager) {
  DCHECK_EQ(infobar_manager_, manager);
  infobar_ = nullptr;
  infobar_manager_->RemoveObserver(this);
  infobar_manager_ = nullptr;
}

// static
PinInfoBarController* PinInfoBarController::From(
    BrowserWindowInterface* window) {
  return Get(window->GetUnownedUserDataHost());
}

// static
void PinInfoBarController::MaybeShowInfoBarForBrowser(
    base::WeakPtr<BrowserWindowInterface> browser,
    base::OnceCallback<void(bool)> done_callback,
    bool another_infobar_shown) {
  // Don't show the infobar if a higher priority infobar has been shown or might
  // be about to show, to avoid asking too many similar questions in a session.
  if (another_infobar_shown || !browser) {
    std::move(done_callback).Run(another_infobar_shown);
    return;
  }
  PinInfoBarController* controller = PinInfoBarController::From(browser.get());
  if (controller) {
    controller->MaybeShowInfoBar(std::move(done_callback));
  } else {
    std::move(done_callback).Run(false);
  }
}

void PinInfoBarController::MaybeShowInfoBar(
    base::OnceCallback<void(bool)> done_callback) {
  // Check if Chrome is the default browser.
  scoped_refptr<shell_integration::DefaultBrowserWorker>(
      new shell_integration::DefaultBrowserWorker())
      ->StartCheckIsDefault(
          base::BindOnce(&PinInfoBarController::OnIsDefaultBrowserResult,
                         weak_factory_.GetWeakPtr(), std::move(done_callback)));
}

void PinInfoBarController::OnIsDefaultBrowserResult(
    base::OnceCallback<void(bool)> done_callback,
    shell_integration::DefaultWebClientState default_state) {
  const bool can_proceed =
      (default_state == shell_integration::DefaultWebClientState::IS_DEFAULT)
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
      || base::FeatureList::IsEnabled(features::kSeparateDefaultAndPinPrompt)
#endif
      ;
  if (!can_proceed) {
    std::move(done_callback).Run(false);
    return;
  }
#if BUILDFLAG(IS_WIN)
  // Check if Chrome can be pinned to the taskbar.
  browser_util::ShouldOfferToPin(
      ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
      browser_util::PinAppToTaskbarChannel::kPinToTaskbarInfoBar,
      base::BindOnce(&PinInfoBarController::OnShouldOfferToPinResult,
                     weak_factory_.GetWeakPtr(), std::move(done_callback)));
#elif BUILDFLAG(IS_MAC)
  OnShouldOfferToPinResult(std::move(done_callback), ShouldOfferToPin());
#endif
}

void PinInfoBarController::OnShouldOfferToPinResult(
    base::OnceCallback<void(bool)> done_callback,
    bool should_offer_to_pin) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Only offer to pin if:
  // * it's okay to pin to the taskbar
  // * this is a normal browser window
  // * the current profile is not incognito or a guest
  const auto* profile = browser_->GetProfile();
  if (!should_offer_to_pin ||
      browser_->GetType() != BrowserWindowInterface::TYPE_NORMAL ||
      profile->IsIncognitoProfile() || profile->IsGuestSession()) {
    std::move(done_callback).Run(false);
    return;
  }

  // Don't show the infobar if it's already showing or was recently shown.
  if (infobar_ || infobar_shown_ || InfoBarShownRecentlyOrMaxTimes()) {
    std::move(done_callback).Run(false);
    return;
  }

  if (infobars::IsInfoBarMigrated(
          infobars::InfoBarDelegate::PIN_INFOBAR_DELEGATE)) {
    infobars::BrowserInfoBarManager::From(g_browser_process)
        ->ShowGlobally(infobars::InfoBarDelegate::PIN_INFOBAR_DELEGATE);
    infobar_shown_ = true;
    SetInfoBarShownRecently();
    std::move(done_callback).Run(true);
    return;
  }

  // On startup, the TabStripModel might not have an active WebContents yet.
  content::WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!web_contents) {
    std::move(done_callback).Run(false);
    return;
  }

  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  if (!infobar_manager) {
    std::move(done_callback).Run(false);
    return;
  }

  infobar_ = PinInfoBarDelegate::Create(infobar_manager);
  if (!infobar_) {
    std::move(done_callback).Run(false);
    return;
  }

  // Only start observing after successful infobar creation to avoid leaks
  // if creation fails (e.g., if an identical infobar already exists).
  infobar_manager_ = infobar_manager;
  infobar_manager_->AddObserver(this);
  SetInfoBarShownRecently();
  std::move(done_callback).Run(true);
}

}  // namespace default_browser
