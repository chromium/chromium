// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_iph_controller.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "content/public/browser/web_contents.h"

namespace send_tab_to_self {

DEFINE_USER_DATA(SendTabToSelfIphController);

SendTabToSelfIphController::SendTabToSelfIphController(
    BrowserWindowInterface* interface)
    : browser_window_interface_(interface),
      scoped_data_(interface->GetUnownedUserDataHost(), *this) {
  if (!base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfEnhancedDesktopUI)) {
    return;
  }
  if (TabStripModel* tab_strip_model =
          browser_window_interface_->GetTabStripModel()) {
    tab_strip_model->AddObserver(this);
  }
  if (SendTabToSelfSyncService* sync_service =
          SendTabToSelfSyncServiceFactory::GetForProfile(
              browser_window_interface_->GetProfile())) {
    if (SendTabToSelfModel* model = sync_service->GetSendTabToSelfModel()) {
      model_observation_.Observe(model);
    }
  }
  MaybeShowPromo();
}

SendTabToSelfIphController::~SendTabToSelfIphController() = default;

// static
SendTabToSelfIphController* SendTabToSelfIphController::From(
    BrowserWindowInterface* interface) {
  return interface ? ui::ScopedUnownedUserData<SendTabToSelfIphController>::Get(
                         interface->GetUnownedUserDataHost())
                   : nullptr;
}

void SendTabToSelfIphController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    MaybeShowPromo();
  }
}

void SendTabToSelfIphController::OnTabChangedAt(tabs::TabInterface* tab,
                                                TabChangeType change_type) {
  if (change_type != TabChangeType::kAll) {
    return;
  }

  TabStripModel* tab_strip_model =
      browser_window_interface_->GetTabStripModel();
  if (!tab_strip_model || tab != tab_strip_model->GetActiveTab()) {
    return;
  }
  MaybeShowPromo();
}

void SendTabToSelfIphController::OnModelReady() {
  MaybeShowPromo();
}

void SendTabToSelfIphController::MaybeShowPromo() {
  if (promo_shown_) {
    return;
  }
  if (!base::FeatureList::IsEnabled(
          send_tab_to_self::kSendTabToSelfEnhancedDesktopUI)) {
    return;
  }
  TabStripModel* tab_strip_model =
      browser_window_interface_->GetTabStripModel();
  if (!tab_strip_model) {
    return;
  }

  if (GetEntryPointDisplayReason(tab_strip_model->GetActiveWebContents()) ==
      EntryPointDisplayReason::kOfferFeature) {
    promo_shown_ = true;
    if (BrowserUserEducationInterface* user_education =
            BrowserUserEducationInterface::From(browser_window_interface_)) {
      user_education->MaybeShowStartupFeaturePromo(
          feature_engagement::kIPHSendTabToSelfTutorialFeature);
    }
    TabStripModelObserver::StopObservingAll(this);
    model_observation_.Reset();
  }
}

}  // namespace send_tab_to_self
