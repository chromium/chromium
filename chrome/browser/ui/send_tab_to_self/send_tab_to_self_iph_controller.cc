// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_iph_controller.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/restore_type.h"
#include "ui/base/page_transition_types.h"

namespace send_tab_to_self {

DEFINE_USER_DATA(SendTabToSelfIphController);

SendTabToSelfIphController::SendTabToSelfIphController(
    BrowserWindowInterface* interface)
    : browser_window_interface_(interface),
      scoped_data_(interface->GetUnownedUserDataHost(), *this) {
  TabStripModel* const tab_strip_model =
      browser_window_interface_->GetTabStripModel();
  tab_strip_model->AddObserver(this);
  Observe(tab_strip_model->GetActiveWebContents());
}

SendTabToSelfIphController::~SendTabToSelfIphController() = default;

// static
SendTabToSelfIphController* SendTabToSelfIphController::From(
    BrowserWindowInterface* interface) {
  return interface ? ui::ScopedUnownedUserData<SendTabToSelfIphController>::Get(
                         interface->GetUnownedUserDataHost())
                   : nullptr;
}

bool SendTabToSelfIphController::IsPromoTab(tabs::TabHandle tab) const {
  return promo_tab_ != tabs::TabHandle::Null() && tab == promo_tab_;
}

void SendTabToSelfIphController::SetPromoTab(tabs::TabHandle promo_tab) {
  promo_tab_ = promo_tab;
}

// static
bool SendTabToSelfIphController::IsTabEligible(const tabs::TabInterface* tab) {
  if (!tab) {
    return false;
  }
  return GetEntryPointDisplayReason(tab->GetContents()) ==
         EntryPointDisplayReason::kOfferFeature;
}

void SendTabToSelfIphController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }
  Observe(selection.new_contents);
  // Only user switches count, not e.g. session restore activating a tab.
  if (selection.reason & TabStripModelObserver::CHANGE_REASON_USER_GESTURE) {
    MaybeShowPromo();
  }
}

void SendTabToSelfIphController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  // Session restore and startup pages aren't user navigations.
  if (navigation_handle->GetRestoreType() == content::RestoreType::kRestored ||
      ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                   ui::PAGE_TRANSITION_AUTO_TOPLEVEL)) {
    return;
  }
  // Page-initiated loads (e.g. redirects) count only after a user click.
  if (navigation_handle->IsRendererInitiated() &&
      !navigation_handle->StartedWithTransientActivation()) {
    return;
  }
  MaybeShowPromo();
}

void SendTabToSelfIphController::MaybeShowPromo() {
  if (promo_request_pending_) {
    return;
  }

  std::optional<EntryPointDisplayReason> reason =
      GetEntryPointDisplayReason(web_contents());
  // Wait if the URL is ineligible (e.g. NTP) or sync/model is still loading.
  if (!reason.has_value()) {
    return;
  }

  if (*reason != EntryPointDisplayReason::kOfferFeature) {
    // Once a non-eligible profile state is resolved, tear down observers so
    // this promo does not trigger mid-session (e.g. upon re-auth).
    StopObserving();
    return;
  }

  user_education::FeaturePromoParams params(
      feature_engagement::kIPHSendTabToSelfTutorialFeature);
  params.show_promo_result_callback =
      base::BindOnce(&SendTabToSelfIphController::OnShowPromoResult,
                     weak_ptr_factory_.GetWeakPtr());
  promo_request_pending_ = true;
  BrowserUserEducationInterface::From(browser_window_interface_)
      ->MaybeShowFeaturePromo(std::move(params));
}

void SendTabToSelfIphController::OnShowPromoResult(
    user_education::FeaturePromoResult result) {
  promo_request_pending_ = false;
  // Keep retrying through the grace period.
  if (result != user_education::FeaturePromoResult::kBlockedByGracePeriod) {
    StopObserving();
  }
}

void SendTabToSelfIphController::StopObserving() {
  Observe(nullptr);
  TabStripModelObserver::StopObservingAll(this);
}

}  // namespace send_tab_to_self
