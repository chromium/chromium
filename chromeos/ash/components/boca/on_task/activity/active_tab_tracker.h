// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ACTIVITY_ACTIVE_TAB_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ACTIVITY_ACTIVE_TAB_TRACKER_H_

#include <string>

#include "chromeos/ash/components/boca/boca_window_observer.h"

namespace ash::boca {

class ActiveTabTracker : public boca::BocaWindowObserver {
 public:
  ActiveTabTracker();
  ~ActiveTabTracker() override;

  // BocaWindowObserver:
  void OnActiveTabChanged(const std::u16string& tab_title) override;
};

}  // namespace ash::boca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ACTIVITY_ACTIVE_TAB_TRACKER_H_
