// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ACTIVITY_ACTIVE_TAB_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ACTIVITY_ACTIVE_TAB_TRACKER_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "chromeos/ash/components/boca/boca_window_observer.h"

namespace ash::boca {

class BocaSessionManager;

class ActiveTabTracker : public boca::BocaWindowObserver {
 public:
  explicit ActiveTabTracker(BocaSessionManager* boca_session_manager);
  ~ActiveTabTracker() override;

  // BocaWindowObserver:
  void OnActiveTabChanged(const std::u16string& tab_title) override;

 private:
  const raw_ref<BocaSessionManager> boca_session_manager_;
};

}  // namespace ash::boca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ON_TASK_ACTIVITY_ACTIVE_TAB_TRACKER_H_
