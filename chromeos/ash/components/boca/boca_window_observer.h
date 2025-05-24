// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_WINDOW_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_WINDOW_OBSERVER_H_

#include <string>

#include "base/observer_list_types.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace ash::boca {
class BocaWindowObserver : public base::CheckedObserver {
 public:
  BocaWindowObserver(const BocaWindowObserver&) = delete;
  BocaWindowObserver& operator=(const BocaWindowObserver&) = delete;

  ~BocaWindowObserver() override = default;

  // Notifies when new tabs are added.
  virtual void OnTabAdded(const SessionID active_tab_id,
                          const SessionID tab_id,
                          const GURL url) {}

  // Notifies when tabs are removed.
  virtual void OnTabRemoved(const SessionID tab_id) {}

  // Notifies when the active tab changes. Just include basic tab info, tab
  // model can't be carried into chromeos dir.
  virtual void OnActiveTabChanged(const std::u16string& tab_title) {}

  // Notifies when the window tracker is cleaned up.
  virtual void OnWindowTrackerCleanedup() {}

 protected:
  BocaWindowObserver() = default;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_WINDOW_OBSERVER_H_
