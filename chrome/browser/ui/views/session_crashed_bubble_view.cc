// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

enum SessionCrashedBubbleHistogramValue {
  SESSION_CRASHED_BUBBLE_SHOWN,
  SESSION_CRASHED_BUBBLE_ERROR,
  SESSION_CRASHED_BUBBLE_RESTORED,
  SESSION_CRASHED_BUBBLE_ALREADY_UMA_OPTIN,
  SESSION_CRASHED_BUBBLE_UMA_OPTIN,
  SESSION_CRASHED_BUBBLE_HELP,
  SESSION_CRASHED_BUBBLE_IGNORED,
  SESSION_CRASHED_BUBBLE_OPTIN_BAR_SHOWN,
  SESSION_CRASHED_BUBBLE_STARTUP_PAGES,
  SESSION_CRASHED_BUBBLE_MAX,
};

void RecordBubbleHistogramValue(SessionCrashedBubbleHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION(
      "SessionCrashed.Bubble", value, SESSION_CRASHED_BUBBLE_MAX);
}

bool DoesSupportConsentCheck() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}

void OpenUmaLink(Browser* browser, const ui::Event& event) {
  browser->OpenURL(content::OpenURLParams(
      GURL("https://support.google.com/chrome/answer/96817"),
      content::Referrer(),
      ui::DispositionFromEventFlags(event.flags(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB),
      ui::PAGE_TRANSITION_LINK, false));
  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_HELP);
}

constexpr int kUmaConsentCheckboxId = 1;

class SessionCrashedBubbleDelegate : public ui::DialogModelDelegate {
 public:
  void OpenStartupPages(Browser* browser) {
    ignored_ = false;

    MaybeEnableUma();
    dialog_model()->host()->Close();

    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_STARTUP_PAGES);
    // Opening tabs has side effects, so it's preferable to do it after the
    // bubble was closed.
    SessionRestore::OpenStartupPagesAfterCrash(browser);
  }

  void OnWindowClosing() {
    if (ignored_)
      RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_IGNORED);
  }

  void RestorePreviousSession(Browser* browser) {
    ignored_ = false;
    MaybeEnableUma();
    dialog_model()->host()->Close();

    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_RESTORED);
    // Restoring tabs has side effects, so it's preferable to do it after the
    // bubble was closed.
    SessionRestore::RestoreSessionAfterCrash(browser);
  }

  void MaybeEnableUma() {
    // Record user's choice for opt-in in to UMA.
    // There's no opt-out choice in the crash restore bubble.
    if (!dialog_model()->HasField(kUmaConsentCheckboxId))
      return;

    if (dialog_model()
            ->GetCheckboxByUniqueId(kUmaConsentCheckboxId)
            ->is_checked()) {
      ChangeMetricsReportingState(true);
      RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_UMA_OPTIN);
    }
  }

 private:
  bool ignored_ = true;
};

}  // namespace

// A helper class that listens to browser removal event.
class SessionCrashedBubbleView::BrowserRemovalObserver
    : public BrowserListObserver {
 public:
  explicit BrowserRemovalObserver(Browser* browser) : browser_(browser) {
    DCHECK(browser_);
    BrowserList::AddObserver(this);
  }

  ~BrowserRemovalObserver() override { BrowserList::RemoveObserver(this); }

  // Overridden from BrowserListObserver.
  void OnBrowserRemoved(Browser* browser) override {
    if (browser == browser_)
      browser_ = nullptr;
  }

  Browser* browser() const { return browser_; }

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserRemovalObserver);
};

// static
void SessionCrashedBubble::ShowIfNotOffTheRecordProfile(Browser* browser) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (browser->profile()->IsOffTheRecord() ||
      browser->profile()->IsEphemeralGuestProfile()) {
    return;
  }

  // Observes possible browser removal before Show is called.
  auto browser_observer =
      std::make_unique<SessionCrashedBubbleView::BrowserRemovalObserver>(
          browser);

  if (DoesSupportConsentCheck()) {
    base::PostTaskAndReplyWithResult(
        GoogleUpdateSettings::CollectStatsConsentTaskRunner(), FROM_HERE,
        base::BindOnce(&GoogleUpdateSettings::GetCollectStatsConsent),
        base::BindOnce(&SessionCrashedBubbleView::Show,
                       std::move(browser_observer)));
  } else {
    SessionCrashedBubbleView::Show(std::move(browser_observer), false);
  }
}

// static
void SessionCrashedBubbleView::Show(
    std::unique_ptr<BrowserRemovalObserver> browser_observer,
    bool uma_opted_in_already) {
  // Determine whether or not the UMA opt-in option should be offered. It is
  // offered only when it is a Google chrome build, user hasn't opted in yet,
  // and the preference is modifiable by the user.
  bool offer_uma_optin = false;

  if (DoesSupportConsentCheck() && !uma_opted_in_already)
    offer_uma_optin = !IsMetricsReportingPolicyManaged();

  Browser* browser = browser_observer->browser();

  if (!browser || !browser->tab_strip_model()->GetActiveWebContents()) {
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_ERROR);
    return;
  }

  ShowBubble(browser, uma_opted_in_already, offer_uma_optin);
}

views::BubbleDialogDelegateView* SessionCrashedBubbleView::ShowBubble(
    Browser* browser,
    bool uma_opted_in_already,
    bool offer_uma_optin) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::SESSION_CRASHED);

  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar_button_provider()
                                 ->GetAppMenuButton();

  auto bubble_delegate_unique =
      std::make_unique<SessionCrashedBubbleDelegate>();
  SessionCrashedBubbleDelegate* bubble_delegate = bubble_delegate_unique.get();

  ui::DialogModel::Builder dialog_builder(std::move(bubble_delegate_unique));
  dialog_builder
      .SetTitle(l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_BUBBLE_TITLE))
      .DisableCloseOnDeactivate()
      .SetIsAlertDialog()
      .SetWindowClosingCallback(
          base::BindOnce(&SessionCrashedBubbleDelegate::OnWindowClosing,
                         base::Unretained(bubble_delegate)))
      .AddBodyText(ui::DialogModelLabel(IDS_SESSION_CRASHED_VIEW_MESSAGE));

  if (offer_uma_optin) {
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_OPTIN_BAR_SHOWN);

    dialog_builder.AddCheckbox(
        kUmaConsentCheckboxId,
        ui::DialogModelLabel::CreateWithLink(
            IDS_SESSION_CRASHED_VIEW_UMA_OPTIN,
            ui::DialogModelLabel::Link(
                IDS_SESSION_CRASHED_BUBBLE_UMA_LINK_TEXT,
                base::BindRepeating(&OpenUmaLink, browser)))
            .set_is_secondary());
  }

  const SessionStartupPref session_startup_pref =
      SessionStartupPref::GetStartupPref(browser->profile());

  if (session_startup_pref.type == SessionStartupPref::URLS &&
      !session_startup_pref.urls.empty()) {
    dialog_builder.AddCancelButton(
        base::BindOnce(&SessionCrashedBubbleDelegate::OpenStartupPages,
                       base::Unretained(bubble_delegate), browser));
  }

  dialog_builder.AddOkButton(
      base::BindOnce(&SessionCrashedBubbleDelegate::RestorePreviousSession,
                     base::Unretained(bubble_delegate), browser),
      l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON));

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_builder.Build(), anchor_view, views::BubbleBorder::TOP_RIGHT);

  views::BubbleDialogDelegateView* bubble_ptr = bubble.get();
  views::BubbleDialogDelegateView::CreateBubble(bubble.release())->Show();

  RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_SHOWN);
  if (uma_opted_in_already)
    RecordBubbleHistogramValue(SESSION_CRASHED_BUBBLE_ALREADY_UMA_OPTIN);
  return bubble_ptr;
}
