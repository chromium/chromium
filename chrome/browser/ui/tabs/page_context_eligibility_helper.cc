// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

DEFINE_USER_DATA(PageContextEligibilityHelper);

PageContextEligibilityHelper::PageContextEligibilityHelper(
    tabs::TabInterface& tab)
    : tabs::ContentsObservingTabFeature(tab),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  tab_subscriptions_.push_back(tab.RegisterDidActivate(
      base::BindRepeating(&PageContextEligibilityHelper::OnTabActivated,
                          weak_ptr_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab.RegisterWillDeactivate(
      base::BindRepeating(&PageContextEligibilityHelper::OnTabDeactivated,
                          weak_ptr_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab.RegisterWillDetach(
      base::BindRepeating(&PageContextEligibilityHelper::OnWillDetach,
                          weak_ptr_factory_.GetWeakPtr())));
  if (tab.IsActivated()) {
    OnTabActivated(&tab);
  }
}

PageContextEligibilityHelper::~PageContextEligibilityHelper() = default;

// static
PageContextEligibilityHelper* PageContextEligibilityHelper::From(
    tabs::TabInterface* tab) {
  if (!tab) {
    return nullptr;
  }
  return Get(tab->GetUnownedUserDataHost());
}

optimization_guide::PageContextEligibilityStatus
PageContextEligibilityHelper::IsPageContextEligible() const {
  if (!observer_) {
    return optimization_guide::PageContextEligibilityStatus::kUnknown;
  }
  return observer_->IsPageContextEligible();
}

base::CallbackListSubscription
PageContextEligibilityHelper::RegisterEligibilityChangeCallback(
    EligibilityChangeCallback callback) {
  return callbacks_.Add(std::move(callback));
}

void PageContextEligibilityHelper::OnDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  tabs::ContentsObservingTabFeature::OnDiscardContents(tab, old_contents,
                                                       new_contents);
  ResetObserverAndNotifyUnknown();
  if (tab->IsActivated()) {
    CreateObserver(new_contents);
  }
}

void PageContextEligibilityHelper::OnTabActivated(tabs::TabInterface* tab) {
  if (!observer_) {
    CreateObserver(tab->GetContents());
  } else {
    NotifyEligibilityChanged(IsPageContextEligible());
  }
}

void PageContextEligibilityHelper::OnTabDeactivated(tabs::TabInterface* tab) {
  ResetObserverAndNotifyUnknown();
}

void PageContextEligibilityHelper::OnWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    ResetObserverAndNotifyUnknown();
  }
}

void PageContextEligibilityHelper::ResetObserverAndNotifyUnknown() {
  observer_.reset();
  NotifyEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus::kUnknown);
}

void PageContextEligibilityHelper::OnEligibilityChanged(
    optimization_guide::PageContextEligibilityStatus status) {
  NotifyEligibilityChanged(status);
}

void PageContextEligibilityHelper::CreateObserver(
    content::WebContents* contents) {
  if (!contents) {
    return;
  }
  observer_ = optimization_guide::PageContextEligibilityObserver::Create(
      contents, GetAccountEmail(),
      base::BindRepeating(&PageContextEligibilityHelper::OnEligibilityChanged,
                          weak_ptr_factory_.GetWeakPtr()));
  NotifyEligibilityChanged(IsPageContextEligible());
}

void PageContextEligibilityHelper::NotifyEligibilityChanged(
    optimization_guide::PageContextEligibilityStatus status) {
  if (last_eligibility_ == status) {
    return;
  }
  last_eligibility_ = status;
  callbacks_.Notify(status);
}

std::string PageContextEligibilityHelper::GetAccountEmail() {
  Profile* profile = tab().GetProfile();
  if (!profile) {
    return std::string();
  }
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return std::string();
  }
  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
      .email;
}

}  // namespace tabs
