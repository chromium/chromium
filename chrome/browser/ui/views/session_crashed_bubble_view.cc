// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_crashed_bubble_view.h"

#include <stddef.h>

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/sessions/session_restore.h"
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
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/buildflags.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/label_button_border.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

views::BubbleDialogDelegate* g_instance_for_test = nullptr;

bool DoesSupportConsentCheck() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return true;
#else
  return false;
#endif
}

void OpenUmaLink(Browser* browser, const ui::Event& event) {
  browser->OpenURL(
      content::OpenURLParams(
          GURL("https://support.google.com/chrome/answer/96817"),
          content::Referrer(),
          ui::DispositionFromEventFlags(
              event.flags(), WindowOpenDisposition::NEW_FOREGROUND_TAB),
          ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kUmaConsentCheckboxId);

class SessionCrashedBubbleDelegate : public ui::DialogModelDelegate {
 public:
  explicit SessionCrashedBubbleDelegate(Profile* profile) {
    if (ExitTypeService* exit_type_service =
            ExitTypeService::GetInstanceForProfile(profile)) {
      crashed_lock_ = exit_type_service->CreateCrashedLock();
    }
  }

  ~SessionCrashedBubbleDelegate() override { g_instance_for_test = nullptr; }

  void OpenStartupPages(Browser* browser) {
    MaybeEnableUma();
    dialog_model()->host()->Close();

    // Opening tabs has side effects, so it's preferable to do it after the
    // bubble was closed.
    SessionRestore::OpenStartupPagesAfterCrash(browser);
  }

  void RestorePreviousSession(Browser* browser) {
    MaybeEnableUma();
    // The call to Close() deletes this. Grab the lock so that session restore
    // is triggered before the lock is destroyed, otherwise ExitTypeService
    // won't wait for restore to complete.
    std::unique_ptr<ExitTypeService::CrashedLock> lock =
        std::move(crashed_lock_);
    dialog_model()->host()->Close();

    // Restoring tabs has side effects, so it's preferable to do it after the
    // bubble was closed.
    SessionRestore::RestoreSessionAfterCrash(browser);
  }

  void MaybeEnableUma() {
    // Record user's choice for opt-in in to UMA.
    // There's no opt-out choice in the crash restore bubble.
    if (!dialog_model()->HasField(kUmaConsentCheckboxId)) {
      return;
    }

    if (dialog_model()
            ->GetCheckboxByUniqueId(kUmaConsentCheckboxId)
            ->is_checked()) {
      ChangeMetricsReportingState(true);
    }
  }

 private:
  std::unique_ptr<ExitTypeService::CrashedLock> crashed_lock_;
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

  BrowserRemovalObserver(const BrowserRemovalObserver&) = delete;
  BrowserRemovalObserver& operator=(const BrowserRemovalObserver&) = delete;

  ~BrowserRemovalObserver() override { BrowserList::RemoveObserver(this); }

  // Overridden from BrowserListObserver.
  void OnBrowserRemoved(Browser* browser) override {
    if (browser == browser_) {
      browser_ = nullptr;
    }
  }

  Browser* browser() const { return browser_; }

 private:
  raw_ptr<Browser> browser_;
};

// static
void SessionCrashedBubble::ShowIfNotOffTheRecordProfile(
    Browser* browser,
    bool skip_tab_checking) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (browser->profile()->IsOffTheRecord()) {
    return;
  }

  // Observes possible browser removal before Show is called.
  auto browser_observer =
      std::make_unique<SessionCrashedBubbleView::BrowserRemovalObserver>(
          browser);

  if (DoesSupportConsentCheck()) {
    GoogleUpdateSettings::CollectStatsConsentTaskRunner()
        ->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(&GoogleUpdateSettings::GetCollectStatsConsent),
            base::BindOnce(&SessionCrashedBubbleView::Show,
                           std::move(browser_observer), skip_tab_checking));
  } else {
    SessionCrashedBubbleView::Show(std::move(browser_observer),
                                   skip_tab_checking, false);
  }
}

// static
void SessionCrashedBubbleView::Show(
    std::unique_ptr<BrowserRemovalObserver> browser_observer,
    bool skip_tab_checking,
    bool uma_opted_in_already) {
  // Determine whether or not the UMA opt-in option should be offered. It is
  // offered only when it is a Google chrome build, user hasn't opted in yet,
  // and the preference is modifiable by the user.
  bool offer_uma_optin = false;

  if (DoesSupportConsentCheck() && !uma_opted_in_already) {
    offer_uma_optin = !IsMetricsReportingPolicyManaged();
  }

  Browser* browser = browser_observer->browser();

  if (browser && (skip_tab_checking ||
                  browser->tab_strip_model()->GetActiveWebContents())) {
    ShowBubble(browser, offer_uma_optin);
    return;
  }
}

// static
views::BubbleDialogDelegate* SessionCrashedBubbleView::GetInstanceForTest() {
  return g_instance_for_test;
}

views::BubbleDialogDelegate* SessionCrashedBubbleView::ShowBubble(
    Browser* browser,
    bool offer_uma_optin) {
  views::View* anchor_view = BrowserView::GetBrowserViewForBrowser(browser)
                                 ->toolbar_button_provider()
                                 ->GetAppMenuButton();

  auto bubble_delegate_unique =
      std::make_unique<SessionCrashedBubbleDelegate>(browser->profile());
  SessionCrashedBubbleDelegate* bubble_delegate = bubble_delegate_unique.get();

  ui::DialogModel::Builder dialog_builder(std::move(bubble_delegate_unique));
  dialog_builder
      .SetTitle(l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_BUBBLE_TITLE))
      .DisableCloseOnDeactivate()
      .SetIsAlertDialog()
      .AddParagraph(ui::DialogModelLabel(IDS_SESSION_CRASHED_VIEW_MESSAGE));

  if (offer_uma_optin) {
    dialog_builder.AddCheckbox(
        kUmaConsentCheckboxId,
        ui::DialogModelLabel::CreateWithReplacement(
            IDS_SESSION_CRASHED_VIEW_UMA_OPTIN,
            ui::DialogModelLabel::CreateLink(
                IDS_SESSION_CRASHED_BUBBLE_UMA_LINK_TEXT,
                base::BindRepeating(&OpenUmaLink, browser)))
            .set_is_secondary());
  }

  const SessionStartupPref session_startup_pref =
      SessionStartupPref::GetStartupPref(browser->profile());

  if (session_startup_pref.ShouldOpenUrls() &&
      !session_startup_pref.urls.empty()) {
    dialog_builder.AddCancelButton(
        base::BindOnce(&SessionCrashedBubbleDelegate::OpenStartupPages,
                       base::Unretained(bubble_delegate), browser));
  }

  dialog_builder.AddOkButton(
      base::BindOnce(&SessionCrashedBubbleDelegate::RestorePreviousSession,
                     base::Unretained(bubble_delegate), browser),
      ui::DialogModel::Button::Params().SetLabel(
          l10n_util::GetStringUTF16(IDS_SESSION_CRASHED_VIEW_RESTORE_BUTTON)));

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_builder.Build(), anchor_view, views::BubbleBorder::TOP_RIGHT);

  views::BubbleDialogDelegate* bubble_ptr = bubble.get();
  g_instance_for_test = bubble_ptr;
  views::BubbleDialogDelegate::CreateBubble(std::move(bubble))->Show();

  return bubble_ptr;
}
