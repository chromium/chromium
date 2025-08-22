// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/infobar/pdf_infobar_controller.h"

#include <optional>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/cstring_view.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/pdf/infobar/pdf_infobar_delegate.h"
#include "chrome/browser/ui/pdf/infobar/pdf_infobar_prefs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/pdf/common/constants.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/install_static/install_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace pdf::infobar {
namespace {

// Returns true if `navigation_handle` is committed, not an error page, and
// contains a PDF.
bool IsLoadedPdf(content::NavigationHandle* navigation_handle) {
  const bool has_committed = navigation_handle->HasCommitted();
  const bool is_error_page = navigation_handle->IsErrorPage();
  auto* web_contents = navigation_handle->GetWebContents();
  const bool is_pdf =
      web_contents && web_contents->GetContentsMimeType() == pdf::kPDFMimeType;
  return has_committed && !is_error_page && is_pdf;
}

// Returns true if `browser` supports being set as default and is a normal,
// non-incognito, non-guest browser.
bool IsAppropriateForInfoBar(BrowserWindowInterface* browser) {
#if BUILDFLAG(IS_WIN)
  // On Windows, some install modes don't support being set as default.
  if (!install_static::SupportsSetAsDefaultBrowser()) {
    return false;
  }
#endif  // BUILDFLAG(IS_WIN)
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return false;
  }
  const auto* profile = browser->GetProfile();
  if (profile->IsIncognitoProfile() || profile->IsGuestSession()) {
    return false;
  }
  return true;
}

}  // namespace

std::optional<bool> PdfInfoBarController::higher_priority_infobar_shown_;

PdfInfoBarController::PdfInfoBarController(BrowserWindowInterface* browser)
    : browser_(browser) {
  CHECK(base::FeatureList::IsEnabled(features::kPdfInfoBar));
  if (!IsAppropriateForInfoBar(browser_)) {
    return;
  }

  browser_subscriptions_.push_back(
      browser_->RegisterBrowserDidClose(base::BindRepeating(
          &PdfInfoBarController::OnBrowserClosed, base::Unretained(this))));

  // This is the entry point to the PDF infobar if it shows when a PDF loads.
  if (features::kPdfInfoBarTrigger.Get() ==
      features::PdfInfoBarTrigger::kPdfLoad) {
    // Register to find out when the active tab changes.
    browser_subscriptions_.push_back(browser_->RegisterActiveTabDidChange(
        base::BindRepeating(&PdfInfoBarController::OnActiveTabChanged,
                            base::Unretained(this))));
  }
}

PdfInfoBarController::~PdfInfoBarController() = default;

// static
void PdfInfoBarController::MaybeShowInfoBarAtStartup(
    base::WeakPtr<BrowserWindowInterface> startup_browser,
    bool higher_priority_infobar_shown) {
  higher_priority_infobar_shown_ = higher_priority_infobar_shown;
  if (!startup_browser) {
    return;
  }
  if (!IsAppropriateForInfoBar(startup_browser.get())) {
    return;
  }
  // This is the entry point to the PDF infobar if it shows at startup.
  if (features::kPdfInfoBarTrigger.Get() ==
      features::PdfInfoBarTrigger::kStartup) {
    startup_browser->GetFeatures().pdf_infobar_controller()->MaybeShowInfoBar();
  }
}

void PdfInfoBarController::OnActiveTabChanged(BrowserWindowInterface* browser) {
  // Observe web contents to see if it loads a PDF (see
  // `DidFinishNavigation()`).
  Observe(browser->GetActiveTabInterface()->GetContents());
}

void PdfInfoBarController::OnBrowserClosed(BrowserWindowInterface* browser) {
  if (infobar_) {
    CHECK(infobar_scoped_observation_.IsObserving());
    infobar_scoped_observation_.GetSource()->RemoveInfoBar(infobar_);
  }
}

void PdfInfoBarController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  CHECK(features::kPdfInfoBarTrigger.Get() ==
        features::PdfInfoBarTrigger::kPdfLoad);
  if (IsLoadedPdf(navigation_handle)) {
    MaybeShowInfoBar();
  }
}

void PdfInfoBarController::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                            bool animate) {
  if (infobar_ != infobar) {
    return;
  }
  infobar_ = nullptr;
  infobar_scoped_observation_.Reset();
}

void PdfInfoBarController::MaybeShowInfoBar() {
#if BUILDFLAG(IS_MAC)
  auto is_default_pdf_viewer_callback = base::BindOnce(
      &shell_integration::IsDefaultHandlerForUTType, "com.adobe.pdf");
#elif BUILDFLAG(IS_WIN)
  auto is_default_pdf_viewer_callback = base::BindOnce(
      &shell_integration::IsDefaultHandlerForFileExtension, ".pdf");
#else
#error PdfInfoBarController should only be created on Windows or MacOS
#endif
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, std::move(is_default_pdf_viewer_callback),
      base::BindOnce(&PdfInfoBarController::MaybeShowInfoBarCallback,
                     weak_factory_.GetWeakPtr()));
}

void PdfInfoBarController::MaybeShowInfoBarCallback(
    shell_integration::DefaultWebClientState default_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(features::kPdfInfoBar));

  // If Chrome is already the default PDF viewer, do nothing.
  if (default_state == shell_integration::DefaultWebClientState::IS_DEFAULT ||
      default_state ==
          shell_integration::DefaultWebClientState::OTHER_MODE_IS_DEFAULT) {
    return;
  }
  // Don't offer to be the default PDF viewer if Chrome is set not to view PDFs.
  if (IsPdfViewerDisabled(browser_->GetProfile())) {
    return;
  }
  // Don't offer to be the default PDF viewer if default apps are controlled by
  // a policy.
  if (IsDefaultBrowserPolicyControlled()) {
    return;
  }
  // Don't show the infobar if it's already showing or was recently shown.
  if (infobar_) {
    return;
  }
  if (InfoBarShownRecentlyOrMaxTimes()) {
    return;
  }
  // Don't show the infobar if a higher priority infobar has been shown or might
  // be about to show, to avoid asking too many similar questions in a session.
  if (!higher_priority_infobar_shown_.has_value() ||
      higher_priority_infobar_shown_.value()) {
    return;
  }
  content::WebContents* web_contents =
      browser_->GetTabStripModel()->GetActiveWebContents();
  if (!web_contents) {
    return;
  }

  // Show the PDF infobar.
  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  infobar_scoped_observation_.Observe(infobar_manager);
  infobar_ = PdfInfoBarDelegate::Create(infobar_manager);
  SetInfoBarShownRecently();
}

// static
void PdfInfoBarController::SetHigherPriorityInfoBarShownForTesting(
    bool higher_priority_infobar_shown) {
  higher_priority_infobar_shown_ = higher_priority_infobar_shown;
}

}  // namespace pdf::infobar
