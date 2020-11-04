// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/intranet_redirector_state.h"

#include "base/feature_list.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"

namespace omnibox {

IntranetRedirectorBehavior GetInterceptionChecksBehavior(
    const PrefService* pref_service) {
  // Here, setting the policy will always take precedence.
  // If policy is not set, users are eligible for an experiment.
  // The experiment check happens after the policy check to register the users
  // in an A/B Finch group at that point.
  const PrefService::Preference* behavior_pref = nullptr;
  // Expected to exist unless unregistered in tests.
  if (pref_service) {
    behavior_pref =
        pref_service->FindPreference(omnibox::kIntranetRedirectBehavior);
  }
  if (behavior_pref && !behavior_pref->IsDefaultValue()) {
    // If policy has set the value, use it and ignore the experiment.
    // We filter the integer pref value to known policy/setting options.
    int pref_value = behavior_pref->GetValue()->GetInt();
    if (pref_value ==
        static_cast<int>(IntranetRedirectorBehavior::DISABLE_FEATURE))
      return IntranetRedirectorBehavior::DISABLE_FEATURE;
    if (pref_value ==
        static_cast<int>(IntranetRedirectorBehavior::
                             DISABLE_INTERCEPTION_CHECKS_ENABLE_INFOBARS))
      return IntranetRedirectorBehavior::
          DISABLE_INTERCEPTION_CHECKS_ENABLE_INFOBARS;
    if (pref_value ==
        static_cast<int>(IntranetRedirectorBehavior::
                             ENABLE_INTERCEPTION_CHECKS_AND_INFOBARS))
      return IntranetRedirectorBehavior::
          ENABLE_INTERCEPTION_CHECKS_AND_INFOBARS;
  }

  // When the relevant policy is not set, the experiment defaults the feature
  // off.
  if (base::FeatureList::IsEnabled(
          omnibox::kIntranetRedirectBehaviorPolicyRollout)) {
    return IntranetRedirectorBehavior::DISABLE_FEATURE;
  }

  // Current behavior for users not in the experiment rollout.
  return IntranetRedirectorBehavior::ENABLE_INTERCEPTION_CHECKS_AND_INFOBARS;
}

}  // namespace omnibox
