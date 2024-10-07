// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_ACTIVITY_ACTIVE_TAB_TRACKER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_ACTIVITY_ACTIVE_TAB_TRACKER_H_

#include <string>

#include "chromeos/ash/components/boca/session_api/session_client_impl.h"

namespace ash::boca {
class SessionClientImpl;

class ActiveTabTracker {
 public:
  ActiveTabTracker();
  virtual ~ActiveTabTracker();

  // Just include basic tab info, tab model can't be carried into chromeos
  // dir.
  virtual void OnActiveTabChanged(const std::u16string& tab_title);

 private:
  raw_ptr<SessionClientImpl> session_client_impl_;
};

}  // namespace ash::boca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_ACTIVITY_ACTIVE_TAB_TRACKER_H_
