// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subscription_eligibility/subscription_eligibility_service.h"

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"

namespace subscription_eligibility {

const char kForceAiSubscriptionTier[] = "force-ai-subscription-tier";

SubscriptionEligibilityService::SubscriptionEligibilityService(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(kForceAiSubscriptionTier)) {
    int forced_tier;
    if (base::StringToInt(
            command_line->GetSwitchValueASCII(kForceAiSubscriptionTier),
            &forced_tier)) {
      forced_tier_ = forced_tier;
    }
  }
  pref_registrar_.Init(pref_service_);
  pref_registrar_.Add(
      prefs::kAiSubscriptionTier,
      base::BindRepeating(
          &SubscriptionEligibilityService::OnAiSubscriptionTierUpdated,
          base::Unretained(this)));
  pref_registrar_.Add(
      prefs::kSubscriptionBenefits,
      base::BindRepeating(
          &SubscriptionEligibilityService::OnSubscriptionBenefitsUpdated,
          base::Unretained(this)));
}
SubscriptionEligibilityService::~SubscriptionEligibilityService() = default;

int32_t SubscriptionEligibilityService::GetAiSubscriptionTier() const {
  if (forced_tier_.has_value()) {
    return forced_tier_.value();
  }
  return pref_service_->GetInteger(prefs::kAiSubscriptionTier);
}

base::flat_set<std::string>
SubscriptionEligibilityService::GetSubscriptionBenefits() const {
  const base::ListValue& list =
      pref_service_->GetList(prefs::kSubscriptionBenefits);
  std::vector<std::string> benefits;
  benefits.reserve(list.size());
  for (const base::Value& value : list) {
    if (value.is_string()) {
      benefits.push_back(value.GetString());
    }
  }
  return base::flat_set<std::string>(std::move(benefits));
}

void SubscriptionEligibilityService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SubscriptionEligibilityService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::WeakPtr<SubscriptionEligibilityService>
SubscriptionEligibilityService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SubscriptionEligibilityService::OnAiSubscriptionTierUpdated() {
  for (Observer& observer : observers_) {
    observer.OnAiSubscriptionTierUpdated(GetAiSubscriptionTier());
  }
}

void SubscriptionEligibilityService::OnSubscriptionBenefitsUpdated() {
  base::flat_set<std::string> benefits = GetSubscriptionBenefits();
  for (Observer& observer : observers_) {
    observer.OnSubscriptionBenefitsUpdated(benefits);
  }
}

}  // namespace subscription_eligibility
