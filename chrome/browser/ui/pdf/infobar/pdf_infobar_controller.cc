// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/pdf/infobar/pdf_infobar_controller.h"

#include <optional>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
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
#include "chrome/browser/ui/pdf/infobar/pdf_infobar_delegate.h"
#include "chrome/browser/ui/pdf/infobar/pdf_infobar_prefs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "ui/views/win/hwnd_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace pdf::infobar {
namespace {

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

#if BUILDFLAG(IS_WIN)
// static
void PdfInfoBarController::RecordSettingsResult(
    ShellUtil::ShowSystemUIResult result) {
  auto record_settings_result = base::BindOnce(
      [](ShellUtil::ShowSystemUIResult result) {
        switch (result) {
          case ShellUtil::ShowSystemUIResult::kNotShown: {
            PdfInfoBarController::RecordSettingsResultHistogram(
                PdfInfoBarSettingsResult::kNotShown);
            break;
          }
          case ShellUtil::ShowSystemUIResult::kError: {
            PdfInfoBarController::RecordSettingsResultHistogram(
                PdfInfoBarSettingsResult::kError);
            break;
          }
          case ShellUtil::ShowSystemUIResult::kSuccess: {
            PdfInfoBarController::RecordSettingsResultHistogram(
                shell_integration::IsDefaultHandlerForFileExtension(".pdf")
                    ? PdfInfoBarSettingsResult::kSuccess
                    : PdfInfoBarSettingsResult::kSuccessNoChange);
            break;
          }
          case ShellUtil::ShowSystemUIResult::kFallback: {
            PdfInfoBarController::RecordSettingsResultHistogram(
                shell_integration::IsDefaultHandlerForFileExtension(".pdf")
                    ? PdfInfoBarSettingsResult::kFallback
                    : PdfInfoBarSettingsResult::kFallbackNoChange);
          }
        }
      },
      result);

  // Check whether Chrome has been set as default after a short delay, to wait
  // for the user to interact with the settings UI (while the "Select a default
  // app for .pdf files" pop-up only returns after it closes, the fallback
  // "Default apps" page returns right after opening).
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      std::move(record_settings_result), base::Seconds(30));
}

// static
void PdfInfoBarController::RecordSettingsResultHistogram(
    PdfInfoBarSettingsResult result) {
  base::UmaHistogramEnumeration("PDF.InfoBar.SettingsResult", result);
}
#endif  // BUILDFLAG(IS_WIN)

// static
void PdfInfoBarController::RecordUserInteractionHistogram(
    PdfInfoBarUserInteraction interaction) {
  base::UmaHistogramEnumeration("PDF.InfoBar.UserInteraction", interaction);
}

// static
void PdfInfoBarController::SetAsDefaultPdfHandler(
    content::WebContents* web_contents) {
#if BUILDFLAG(IS_MAC)
  shell_integration::SetAsDefaultHandlerForUTType("com.adobe.pdf");
#elif BUILDFLAG(IS_WIN)
  if (!web_contents) {
    return;
  }
  auto* window = web_contents->GetTopLevelNativeWindow();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&ShellUtil::ShowSetDefaultForFileExtensionSystemUI,
                     base::PathService::CheckedGet(base::FILE_EXE),
                     base::wcstring_view(L".pdf"),
                     views::HWNDForNativeWindow(window)),
      base::BindOnce(&PdfInfoBarController::RecordSettingsResult));
#else
#error PdfInfoBarController/Delegate should only be used on Windows or MacOS
#endif
}

// static
PdfInfoBarController* PdfInfoBarController::From(
    BrowserWindowInterface* window) {
  return Get(window->GetUnownedUserDataHost());
}

// static
void PdfInfoBarController::RegisterInfoBarSpec() {
  auto* browser_infobar_manager =
      infobars::BrowserInfoBarManager::From(g_browser_process);
  if (!browser_infobar_manager) {
    return;
  }

  auto spec =
      infobars::InfoBarSpec::Builder(
          infobars::InfoBarDelegate::PDF_INFOBAR_DELEGATE)
          .SetMessageText(l10n_util::GetStringUTF16(IDS_PDF_INFOBAR_TEXT))
          .SetIcon(vector_icons::kProductRefreshIcon)
          .SetScope(infobars::InfoBarScope::kTab)
          .AddOkButton(
              l10n_util::GetStringUTF16(
                  IDS_DEFAULT_BROWSER_INFOBAR_OK_BUTTON_LABEL),
              base::BindRepeating([](content::WebContents* web_contents) {
                if (!web_contents) {
                  return;
                }
                PdfInfoBarController::RecordUserInteractionHistogram(
                    PdfInfoBarUserInteraction::kAccepted);

                tabs::TabInterface* tab =
                    tabs::TabInterface::GetFromContents(web_contents);
                if (tab && tab->GetBrowserWindowInterface()) {
                  auto* controller = PdfInfoBarController::From(
                      tab->GetBrowserWindowInterface());
                  if (controller) {
                    controller->set_action_taken(true);
                  }
                }

                PdfInfoBarController::SetAsDefaultPdfHandler(web_contents);
              }))
          .SetDismissAction(
              base::BindRepeating([](content::WebContents* web_contents) {
                if (!web_contents) {
                  return;
                }
                PdfInfoBarController::RecordUserInteractionHistogram(
                    PdfInfoBarUserInteraction::kDismissed);

                tabs::TabInterface* tab =
                    tabs::TabInterface::GetFromContents(web_contents);
                if (tab && tab->GetBrowserWindowInterface()) {
                  auto* controller = PdfInfoBarController::From(
                      tab->GetBrowserWindowInterface());
                  if (controller) {
                    controller->set_action_taken(true);
                  }
                }
              }))
          .Build();

  browser_infobar_manager->Register(std::move(spec));
}

DEFINE_USER_DATA(PdfInfoBarController);

std::optional<bool> PdfInfoBarController::higher_priority_infobar_shown_;

PdfInfoBarController::PdfInfoBarController(BrowserWindowInterface* browser)
    : browser_(browser),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {
  CHECK(base::FeatureList::IsEnabled(features::kPdfInfoBar));
  if (!IsAppropriateForInfoBar(browser_)) {
    return;
  }

  browser_subscriptions_.push_back(
      browser_->RegisterBrowserDidClose(base::BindRepeating(
          &PdfInfoBarController::OnBrowserClosed, base::Unretained(this))));
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
  PdfInfoBarController* controller =
      PdfInfoBarController::From(startup_browser.get());
  if (controller) {
    controller->MaybeShowInfoBar();
  }
}

void PdfInfoBarController::OnBrowserClosed(BrowserWindowInterface* browser) {
  if (infobar_) {
    CHECK(infobar_scoped_observation_.IsObserving());
    infobar_scoped_observation_.GetSource()->RemoveInfoBar(infobar_);
  }
}

void PdfInfoBarController::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                            bool animate) {
  if (infobar->delegate()->GetIdentifier() !=
      infobars::InfoBarDelegate::PDF_INFOBAR_DELEGATE) {
    return;
  }

  if (infobars::IsInfoBarMigrated(
          infobars::InfoBarDelegate::PDF_INFOBAR_DELEGATE)) {
    if (!action_taken_) {
      PdfInfoBarController::RecordUserInteractionHistogram(
          PdfInfoBarUserInteraction::kIgnored);
    }
  } else {
    if (infobar_ != infobar) {
      return;
    }
    infobar_ = nullptr;
  }

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

  // Don't show the infobar if it's already showing.
  if (infobar_scoped_observation_.IsObserving()) {
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

  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);

  if (infobars::IsInfoBarMigrated(
          infobars::InfoBarDelegate::PDF_INFOBAR_DELEGATE)) {
    // Record the shown metric only for the centralized InfoBarSpec path (since
    // the legacy InfoBarDelegate path records it in `PdfInfoBarDelegate::Create`).
    base::UmaHistogramBoolean("PDF.InfoBar.Shown", true);

    static bool spec_registered = false;
    if (!spec_registered) {
      RegisterInfoBarSpec();
      spec_registered = true;
    }
    action_taken_ = false;
    if (infobar_manager) {
      infobar_scoped_observation_.Observe(infobar_manager);
    }
    auto* browser_infobar_manager =
        infobars::BrowserInfoBarManager::From(g_browser_process);
    if (browser_infobar_manager) {
      browser_infobar_manager->Show(
          web_contents, infobars::InfoBarDelegate::PDF_INFOBAR_DELEGATE);
    }
  } else {
    if (infobar_manager) {
      infobar_scoped_observation_.Observe(infobar_manager);
      infobar_ = PdfInfoBarDelegate::Create(infobar_manager);
    }
  }
  SetInfoBarShownRecently();
}

// static
void PdfInfoBarController::SetHigherPriorityInfoBarShownForTesting(
    bool higher_priority_infobar_shown) {
  higher_priority_infobar_shown_ = higher_priority_infobar_shown;
}

}  // namespace pdf::infobar
