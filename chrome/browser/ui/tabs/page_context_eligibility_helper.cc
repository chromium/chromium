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

std::optional<bool> PageContextEligibilityHelper::IsPageContextEligible()
    const {
  if (!observer_) {
    return std::nullopt;
  }
  auto status = observer_->IsPageContextEligible();
  if (status == optimization_guide::PageContextEligibilityStatus::kUnknown) {
    return std::nullopt;
  }
  return status == optimization_guide::PageContextEligibilityStatus::kEligible;
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
  if (tab->IsActivated()) {
    observer_ = optimization_guide::PageContextEligibilityObserver::Create(
        new_contents, GetAccountEmail(),
        base::BindRepeating(&PageContextEligibilityHelper::OnEligibilityChanged,
                            weak_ptr_factory_.GetWeakPtr()));
    std::optional<bool> eligibility;
    if (observer_) {
      auto status = observer_->IsPageContextEligible();
      if (status !=
          optimization_guide::PageContextEligibilityStatus::kUnknown) {
        eligibility =
            (status ==
             optimization_guide::PageContextEligibilityStatus::kEligible);
      }
    }
    callbacks_.Notify(eligibility);
  }
}

void PageContextEligibilityHelper::OnTabActivated(tabs::TabInterface* tab) {
  if (!observer_ && tab->GetContents()) {
    observer_ = optimization_guide::PageContextEligibilityObserver::Create(
        tab->GetContents(), GetAccountEmail(),
        base::BindRepeating(&PageContextEligibilityHelper::OnEligibilityChanged,
                            weak_ptr_factory_.GetWeakPtr()));
    std::optional<bool> eligibility;
    if (observer_) {
      auto status = observer_->IsPageContextEligible();
      if (status !=
          optimization_guide::PageContextEligibilityStatus::kUnknown) {
        eligibility =
            (status ==
             optimization_guide::PageContextEligibilityStatus::kEligible);
      }
    }
    callbacks_.Notify(eligibility);
  }
}

void PageContextEligibilityHelper::OnTabDeactivated(tabs::TabInterface* tab) {
  observer_.reset();
  callbacks_.Notify(std::nullopt);
}

void PageContextEligibilityHelper::OnEligibilityChanged(bool is_eligible) {
  callbacks_.Notify(is_eligible);
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
