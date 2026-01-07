// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SYNTHETIC_FIELD_TRIAL_SERVICE_DELEGATE_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SYNTHETIC_FIELD_TRIAL_SERVICE_DELEGATE_H_

#include <string_view>

namespace supervised_user {

// Delegate for registering synthetic field trials for supervised users.
class SynteticFieldTrialDelegate {
 public:
  virtual ~SynteticFieldTrialDelegate() = default;
  // Registers a synthetic field trial for the given trial and group in
  // "current" annotation mode.
  // Note: all new calls to this method should get a review from
  // chromium-metrics-reviews@google.com
  virtual void RegisterSyntheticFieldTrial(std::string_view trial_name,
                                           std::string_view group_name) = 0;
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_SUPERVISED_USER_SYNTHETIC_FIELD_TRIAL_SERVICE_DELEGATE_H_
