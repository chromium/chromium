// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt_manager.h"
#include <memory>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/startup/default_browser_infobar_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

// static
DefaultBrowserPromptManager* DefaultBrowserPromptManager::GetInstance() {
  return base::Singleton<DefaultBrowserPromptManager>::get();
}

// static
void DefaultBrowserPromptManager::MaybeJoinDefaultBrowserPromptCohort() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    return;  // Can be null in unit tests;
  }

  std::string active_study_group =
      features::kDefaultBrowserPromptRefreshStudyGroup.Get();
  // If the study group isn't set, don't add the user to the cohort.
  if (active_study_group.empty()) {
    return;
  }

  local_state->SetString(prefs::kDefaultBrowserPromptRefreshStudyGroup,
                         active_study_group);
  DefaultBrowserPromptManager::RegisterSyntheticFieldTrial(active_study_group);
}

// static
void DefaultBrowserPromptManager::EnsureStickToDefaultBrowserPromptCohort() {
  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    return;  // Can be null in unit tests;
  }

  auto enrolled_study_group =
      local_state->GetString(prefs::kDefaultBrowserPromptRefreshStudyGroup);
  if (enrolled_study_group.empty()) {
    // The user was not enrolled or exited the study at some point.
    return;
  }

  DefaultBrowserPromptManager::RegisterSyntheticFieldTrial(
      enrolled_study_group);
}

DefaultBrowserPromptManager::DefaultBrowserPromptManager() = default;

DefaultBrowserPromptManager::~DefaultBrowserPromptManager() = default;

void DefaultBrowserPromptManager::ShowPrompt() {
  browser_tab_strip_tracker_ =
      std::make_unique<BrowserTabStripTracker>(this, this);
  browser_tab_strip_tracker_->Init();
}

void DefaultBrowserPromptManager::CreateInfoBarForWebContents(
    content::WebContents* web_contents,
    Profile* profile) {
  // Ensure that an infobar hasn't already been created.
  CHECK(!infobars_.contains(web_contents));

  infobars::InfoBar* infobar = chrome::DefaultBrowserInfoBarDelegate::Create(
      infobars::ContentInfoBarManager::FromWebContents(web_contents), profile);
  infobars_[web_contents] = infobar;

  static_cast<ConfirmInfoBarDelegate*>(infobar->delegate())->AddObserver(this);

  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  infobar_manager->AddObserver(this);
}

void DefaultBrowserPromptManager::CloseAllInfoBars() {
  browser_tab_strip_tracker_.reset();

  for (const auto& infobars_entry : infobars_) {
    infobars_entry.second->owner()->RemoveObserver(this);
    infobars_entry.second->RemoveSelf();
  }

  infobars_.clear();
}

bool DefaultBrowserPromptManager::ShouldTrackBrowser(Browser* browser) {
  return browser->is_type_normal() &&
         !browser->profile()->IsIncognitoProfile() &&
         !browser->profile()->IsGuestSession();
}

void DefaultBrowserPromptManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents) {
      if (!base::Contains(infobars_, contents.contents)) {
        CreateInfoBarForWebContents(contents.contents,
                                    tab_strip_model->profile());
      }
    }
  }
}

void DefaultBrowserPromptManager::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                                   bool animate) {
  auto infobars_entry = base::ranges::find(
      infobars_, infobar, &decltype(infobars_)::value_type::second);
  if (infobars_entry == infobars_.end()) {
    return;
  }

  infobar->owner()->RemoveObserver(this);
  infobars_.erase(infobars_entry);
  static_cast<ConfirmInfoBarDelegate*>(infobar->delegate())
      ->RemoveObserver(this);

  if (user_initiated_close_pending_) {
    CloseAllInfoBars();
    user_initiated_close_pending_ = false;
  }
}

void DefaultBrowserPromptManager::OnAccept() {
  base::UmaHistogramCounts100("DefaultBrowser.InfoBar.TimesShownBeforeAccept",
                              g_browser_process->local_state()->GetInteger(
                                  prefs::kDefaultBrowserDeclinedCount) +
                                  1);
  user_initiated_close_pending_ = true;
}

void DefaultBrowserPromptManager::OnDismiss() {
  user_initiated_close_pending_ = true;
}

// static
void DefaultBrowserPromptManager::RegisterSyntheticFieldTrial(
    const std::string& group_name) {
  CHECK(!group_name.empty());

  ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
      "DefaultBrowserPromptRefreshSynthetic", group_name,
      variations::SyntheticTrialAnnotationMode::kCurrentLog);
}
