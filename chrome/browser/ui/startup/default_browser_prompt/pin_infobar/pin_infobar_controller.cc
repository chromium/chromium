// Copyright 2025 The Chromium Authors
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
#include "components/tabs/public/tab_interface.h"
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
#endif  // #if BUILDFLAG(IS_MAC)

namespace default_browser {

PinInfoBarController::PinInfoBarController(BrowserWindowInterface* browser)
    : browser_(browser) {
  CHECK(base::FeatureList::IsEnabled(features::kOfferPinToTaskbarInfoBar));
  browser_subscriptions_.push_back(
      browser_->RegisterBrowserDidClose(base::BindRepeating(
          &PinInfoBarController::OnBrowserClosed, base::Unretained(this))));
}

PinInfoBarController::~PinInfoBarController() = default;

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

// static
void PinInfoBarController::MaybeShowInfoBarForBrowser(
    base::WeakPtr<BrowserWindowInterface> browser,
    base::OnceCallback<void(bool)> done_callback,
    bool another_infobar_shown) {
  // Don't show the infobar if a higher priority infobar has been shown or might
  // be about to show, to avoid asking too many similar questions in a session.
  if (another_infobar_shown || !browser) {
    std::move(done_callback).Run(false);
    return;
  }
  browser->GetFeatures().pin_infobar_controller()->MaybeShowInfoBar(
      std::move(done_callback));
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
  if (default_state != shell_integration::DefaultWebClientState::IS_DEFAULT) {
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
  if (infobar_ || InfoBarShownRecentlyOrMaxTimes()) {
    std::move(done_callback).Run(false);
    return;
  }

  // Show the pin-to-taskbar infobar.
  content::WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  infobar_manager_ =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  infobar_manager_->AddObserver(this);
  infobar_ = PinInfoBarDelegate::Create(infobar_manager_);
  SetInfoBarShownRecently();
  std::move(done_callback).Run(true);
}

}  // namespace default_browser
