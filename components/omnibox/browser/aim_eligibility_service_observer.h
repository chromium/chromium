// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_OBSERVER_H_
#define COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

// Observer interface for AIM eligibility changes.
class AimEligibilityServiceObserver : public base::CheckedObserver {
 public:
  ~AimEligibilityServiceObserver() override = default;
  virtual void OnAimEligibilityChanged() = 0;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_AIM_ELIGIBILITY_SERVICE_OBSERVER_H_
