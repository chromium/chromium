// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_launch_infobar_manager_impl.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/startup/startup_launch_infobar_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/prefs/pref_service.h"

using InfoBarType = StartupLaunchInfoBarManager::InfoBarType;

namespace {

constexpr char histogram_name_base[] = "Startup.Launch.InfoBar";

std::string GetInfoBarTypeVariant(InfoBarType type) {
  switch (type) {
    case InfoBarType::kForegroundOptIn:
      return "ForegroundOptIn";
    case InfoBarType::kForegroundOptOut:
      return "ForegroundOptOut";
  }
}

std::string GetInteractionHistogramName(InfoBarType type) {
  return base::JoinString(
      {
          histogram_name_base,
          GetInfoBarTypeVariant(type),
          "Interaction",
      },
      ".");
}

std::string GetShownHistogramName(InfoBarType type) {
  return base::JoinString(
      {
          histogram_name_base,
          GetInfoBarTypeVariant(type),
          "Shown",
      },
      ".");
}

}  // namespace

StartupLaunchInfoBarManagerImpl::StartupLaunchInfoBarManagerImpl() = default;
StartupLaunchInfoBarManagerImpl::~StartupLaunchInfoBarManagerImpl() {
  CloseAllInfoBars();
}

void StartupLaunchInfoBarManagerImpl::ShowInfoBars(InfoBarType infobar_type) {
  CloseAllInfoBars();

  base::UmaHistogramCounts100(GetShownHistogramName(infobar_type), 1);

  infobar_type_ = infobar_type;
  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
  browser_tab_strip_tracker_ =
      std::make_unique<BrowserTabStripTracker>(this, this);
  // This will trigger a call to `OnTabStripModelChanged`, which will create
  // the info bar.
  browser_tab_strip_tracker_->Init();
}

void StartupLaunchInfoBarManagerImpl::CloseAllInfoBars() {
  did_user_interact_ = false;

  browser_collection_observation_.Reset();
  browser_tab_strip_tracker_.reset();

  // Extract InfoBars to a vector avoid DanglingPtr issues in the map when the
  // InfoBars are destroyed.
  std::vector<infobars::InfoBar*> infobars_to_close;
  infobars_to_close.reserve(infobars_.size());
  for (const auto& entry : infobars_) {
    infobars_to_close.push_back(entry.second);
  }

  // Clear the map before destroying the InfoBars to ensure the
  // raw_ptr<InfoBar>s in the map are destroyed while the InfoBars are still
  // alive.
  infobars_.clear();

  for (infobars::InfoBar* infobar : infobars_to_close) {
    if (!infobar) {
      continue;
    }
    infobar->owner()->RemoveObserver(this);
    infobar->RemoveSelf();
  }
}

void StartupLaunchInfoBarManagerImpl::AddObserver(
    StartupLaunchInfoBarManager::Observer* observer) {
  observers_.AddObserver(observer);
}

void StartupLaunchInfoBarManagerImpl::RemoveObserver(
    StartupLaunchInfoBarManager::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void StartupLaunchInfoBarManagerImpl::CreateInfoBarForWebContents(
    content::WebContents* web_contents,
    Profile* profile) {
  // Ensure that an infobar hasn't already been created.
  CHECK(!infobars_.contains(web_contents));

  auto delegate = StartupLaunchInfoBarDelegate::Create(profile, infobar_type_);
  delegate->AddObserver(this);

  auto* content_infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  infobars::InfoBar* infobar = content_infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::move(delegate)));

  if (infobar == nullptr) {
    // Infobar may be null if `InfoBarManager::ShouldShowInfoBar` returns false,
    // in which case this function should do nothing. One case where this can
    // happen is if the --headless command  line switch is present.
    return;
  }

  infobars_[web_contents] = infobar;
  content_infobar_manager->AddObserver(this);
}

bool StartupLaunchInfoBarManagerImpl::ShouldTrackBrowser(
    BrowserWindowInterface* browser) {
  return browser->GetType() == BrowserWindowInterface::TYPE_NORMAL &&
         !browser->GetProfile()->IsIncognitoProfile() &&
         !browser->GetProfile()->IsGuestSession();
}

void StartupLaunchInfoBarManagerImpl::OnTabStripModelChanged(
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

void StartupLaunchInfoBarManagerImpl::OnInfoBarRemoved(
    infobars::InfoBar* infobar,
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

  if (did_user_interact_) {
    CloseAllInfoBars();
    for (auto& observer : observers_) {
      observer.OnInfoBarDismissed();
    }
  }
}

void StartupLaunchInfoBarManagerImpl::OnAccept() {
  did_user_interact_ = true;
  base::UmaHistogramEnumeration(GetInteractionHistogramName(infobar_type_),
                                StartupLaunchInfoBarInteraction::kAccept);
  g_browser_process->local_state()->SetBoolean(
      prefs::kStartupLaunchInfobarAccepted, true);

  switch (infobar_type_) {
    case InfoBarType::kForegroundOptOut:
      GlobalBrowserCollection::GetInstance()->ForEach(
          [this](BrowserWindowInterface* browser) {
            if (ShouldTrackBrowser(browser)) {
              chrome::ShowSettingsSubPage(browser->GetBrowserForMigrationOnly(),
                                          chrome::kOnStartupSubPage);
              return false;
            }
            return true;
          },
          BrowserCollection::Order::kActivation);
      CloseAllInfoBars();
      break;
    case InfoBarType::kForegroundOptIn:
      g_browser_process->local_state()->SetBoolean(
          prefs::kForegroundLaunchOnLogin, true);
      break;
  }
}

void StartupLaunchInfoBarManagerImpl::OnDismiss() {
  did_user_interact_ = true;
  base::UmaHistogramEnumeration(GetInteractionHistogramName(infobar_type_),
                                StartupLaunchInfoBarInteraction::kDismiss);

  auto* local_state = g_browser_process->local_state();

  const int decline_count =
      local_state->GetInteger(prefs::kStartupLaunchInfobarDeclinedCount);
  local_state->SetInteger(prefs::kStartupLaunchInfobarDeclinedCount,
                          decline_count + 1);

  local_state->SetTime(prefs::kStartupLaunchInfobarLastDeclinedTime,
                       base::Time::Now());
}
