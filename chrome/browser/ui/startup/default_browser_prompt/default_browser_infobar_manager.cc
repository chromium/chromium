// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_manager.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/default_browser/default_browser_controller.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/infobars/browser_infobar_manager.h"
#include "chrome/browser/infobars/infobar_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_infobar_delegate.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_manager.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt_prefs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

using CloseReason = DefaultBrowserPromptManager::CloseReason;

namespace {
bool IsMigrated() {
  return infobars::IsInfoBarMigrated(
      infobars::InfoBarDelegate::DEFAULT_BROWSER_INFOBAR_DELEGATE);
}

// CloseAllPrompts() destroys this surface manager, and the result callback
// runs while an infobar delegate is still on the stack, so the close must
// not happen synchronously.
void CloseAllPromptsSoon(CloseReason reason) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](CloseReason reason) {
            DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(reason);
          },
          reason));
}
}  // namespace

DefaultBrowserInfoBarManager::DefaultBrowserInfoBarManager() = default;

DefaultBrowserInfoBarManager::~DefaultBrowserInfoBarManager() {
  // The result callback handed to the framework binds `this` unretained, so
  // no instance may outlive us.
  HideInfoBar();
}

void DefaultBrowserInfoBarManager::Show(bool can_pin_to_taskbar) {
  DefaultBrowserSurfaceManager::Show(can_pin_to_taskbar);

  if (IsMigrated()) {
    auto* browser_infobar_manager =
        infobars::BrowserInfoBarManager::From(g_browser_process);
    if (browser_infobar_manager) {
      infobars::InfoBarShowParams params;
      if (can_pin_to_taskbar) {
        params.message_text =
            l10n_util::GetStringUTF16(IDS_DEFAULT_BROWSER_PIN_INFOBAR_TEXT);
      }
      params.result_callback =
          base::BindRepeating(&DefaultBrowserInfoBarManager::OnInfoBarResult,
                              base::Unretained(this));
      browser_infobar_manager->ShowGlobally(
          infobars::InfoBarDelegate::DEFAULT_BROWSER_INFOBAR_DELEGATE,
          std::move(params));
      return;
    }
  }

  browser_tab_strip_tracker_ =
      std::make_unique<BrowserTabStripTracker>(this, this);
  // This will trigger a call to `OnTabStripModelChanged`, which will create
  // the info bar.
  browser_tab_strip_tracker_->Init();
}

void DefaultBrowserInfoBarManager::ShowForBrowser(
    BrowserWindowInterface* browser) {
  // The BrowserTabStripTracker will handle showing infobars for both existing
  // and newly created browsers, so we don't need to do anything here.
}

void DefaultBrowserInfoBarManager::CloseForBrowser(
    BrowserWindowInterface* browser) {
  if (IsMigrated()) {
    // The framework reports kIgnored when the last instance goes away with
    // its window; OnInfoBarResult records it.
    return;
  }

  if (user_initiated_info_bar_close_pending_.has_value()) {
    return;
  }

  // If the last browser window that we are tracking is getting closed, and the
  // user hasn't interacted with the infobar yet, we record this as IGNORED.
  bool all_tracked_browser_windows_closed = true;
  GlobalBrowserCollection::GetInstance()->ForEach(
      [&all_tracked_browser_windows_closed, this,
       browser](BrowserWindowInterface* bwi) {
        if (bwi != browser && IsBrowserValidForShowing(bwi)) {
          all_tracked_browser_windows_closed = false;
        }
        return all_tracked_browser_windows_closed;
      });

  if (!all_tracked_browser_windows_closed) {
    return;
  }

  // Reset the observers.
  browser_tab_strip_tracker_.reset();

  ProcessIgnore();
}

void DefaultBrowserInfoBarManager::CloseAllPromptInstances() {
  if (IsMigrated()) {
    HideInfoBar();
    return;
  }

  user_initiated_info_bar_close_pending_.reset();

  browser_tab_strip_tracker_.reset();

  for (const auto& infobars_entry : infobars_) {
    infobars_entry.second->owner()->RemoveObserver(this);
    infobars_entry.second->RemoveSelf();
  }

  infobars_.clear();
}

void DefaultBrowserInfoBarManager::CreateInfoBarForWebContents(
    content::WebContents* web_contents,
    Profile* profile) {
  // Ensure that an infobar hasn't already been created.
  CHECK(!infobars_.contains(web_contents));

  infobars::InfoBar* infobar = DefaultBrowserInfoBarDelegate::Create(
      infobars::ContentInfoBarManager::FromWebContents(web_contents), profile,
      can_pin_to_taskbar());

  if (infobar == nullptr) {
    // Infobar may be null if `InfoBarManager::ShouldShowInfoBar` returns false,
    // in which case this function should do nothing. One case where this can
    // happen is if the --headless command  line switch is present.
    return;
  }

  infobars_[web_contents] = infobar;

  static_cast<ConfirmInfoBarDelegate*>(infobar->delegate())->AddObserver(this);

  auto* content_infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  content_infobar_manager->AddObserver(this);
}

bool DefaultBrowserInfoBarManager::ShouldTrackBrowser(
    BrowserWindowInterface* browser) {
  return IsBrowserValidForShowing(browser);
}

void DefaultBrowserInfoBarManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents) {
      if (!infobars_.contains(contents.contents)) {
        CreateInfoBarForWebContents(contents.contents,
                                    tab_strip_model->profile());
      }
    }
  }
}

void DefaultBrowserInfoBarManager::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                                    bool animate) {
  auto infobars_entry = std::ranges::find(
      infobars_, infobar, &decltype(infobars_)::value_type::second);
  if (infobars_entry == infobars_.end()) {
    return;
  }

  infobar->owner()->RemoveObserver(this);
  infobars_.erase(infobars_entry);
  static_cast<ConfirmInfoBarDelegate*>(infobar->delegate())
      ->RemoveObserver(this);

  if (user_initiated_info_bar_close_pending_.has_value()) {
    // Prompt manager will proceed to close all infobars.
    DefaultBrowserPromptManager::GetInstance()->CloseAllPrompts(
        user_initiated_info_bar_close_pending_.value());
  }
}

void DefaultBrowserInfoBarManager::OnAccept() {
  ProcessAccept();
  user_initiated_info_bar_close_pending_ = CloseReason::kAccept;
}

void DefaultBrowserInfoBarManager::OnDismiss() {
  ProcessDismiss();
  user_initiated_info_bar_close_pending_ = CloseReason::kDismiss;
}

void DefaultBrowserInfoBarManager::OnInfoBarResult(
    content::WebContents* web_contents,
    infobars::InfoBarResult result) {
  Profile* profile =
      web_contents
          ? Profile::FromBrowserContext(web_contents->GetBrowserContext())
          : nullptr;
  switch (result) {
    case infobars::InfoBarResult::kAccepted:
      // The prefs update runs before the count is read, matching the legacy
      // delegate's ordering.
      if (profile) {
        chrome::startup::default_prompt::UpdatePrefsForDismissedPrompt(profile);
      }
      ProcessAccept();
      CloseAllPromptsSoon(CloseReason::kAccept);
      break;
    case infobars::InfoBarResult::kDismissed:
      if (profile) {
        chrome::startup::default_prompt::UpdatePrefsForDismissedPrompt(profile);
      }
      ProcessDismiss();
      CloseAllPromptsSoon(CloseReason::kDismiss);
      break;
    case infobars::InfoBarResult::kIgnored:
      ProcessIgnore();
      // The session counted as ignored; disarm the global infobar so a
      // browser opened later does not resurrect the prompt.
      HideInfoBar();
      break;
    case infobars::InfoBarResult::kCancelled:
    case infobars::InfoBarResult::kLinkClicked:
      // The infobar has no cancel button or link.
      break;
  }
}

void DefaultBrowserInfoBarManager::HideInfoBar() {
  if (!IsMigrated()) {
    return;
  }

  auto* browser_infobar_manager =
      infobars::BrowserInfoBarManager::From(g_browser_process);
  CHECK(browser_infobar_manager);
  browser_infobar_manager->Hide(
      infobars::InfoBarDelegate::DEFAULT_BROWSER_INFOBAR_DELEGATE);
}

void DefaultBrowserInfoBarManager::ProcessAccept() {
  base::UmaHistogramCounts100("DefaultBrowser.InfoBar.TimesShownBeforeAccept",
                              g_browser_process->local_state()->GetInteger(
                                  prefs::kDefaultBrowserInfobarDeclinedCount) +
                                  1);
  base::RecordAction(base::UserMetricsAction("DefaultBrowserInfoBar_Accept"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            ACCEPT_INFO_BAR,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);
  HandleAccept();
}

void DefaultBrowserInfoBarManager::ProcessDismiss() {
  HandleDismiss();
  base::RecordAction(base::UserMetricsAction("DefaultBrowserInfoBar_Dismiss"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            DISMISS_INFO_BAR,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);
}

void DefaultBrowserInfoBarManager::ProcessIgnore() {
  HandleIgnore();
  base::RecordAction(base::UserMetricsAction("DefaultBrowserInfoBar_Ignore"));
  UMA_HISTOGRAM_ENUMERATION("DefaultBrowser.InfoBar.UserInteraction",
                            IGNORE_INFO_BAR_PER_SESSION,
                            NUM_INFO_BAR_USER_INTERACTION_TYPES);
}

default_browser::DefaultBrowserEntrypointType
DefaultBrowserInfoBarManager::GetEntrypointType() const {
  return default_browser::DefaultBrowserEntrypointType::kStartupInfobar;
}
