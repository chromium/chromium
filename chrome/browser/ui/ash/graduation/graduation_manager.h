// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_GRADUATION_GRADUATION_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_GRADUATION_GRADUATION_MANAGER_H_

#include "base/component_export.h"
#include "base/scoped_observation.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace ash::graduation {

// Manages the state of the Graduation app depending on the status of the
// Graduation enablement policy. The GraduationManager is a singleton that
// should be created once per user session.
class COMPONENT_EXPORT(GRADUATION_MANAGER) GraduationManager
    : public session_manager::SessionManagerObserver {
 public:
  GraduationManager();
  GraduationManager(const GraduationManager&) = delete;
  GraduationManager& operator=(const GraduationManager&) = delete;
  ~GraduationManager() override;

  static GraduationManager* Get();

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary) override;

 private:
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};
};
}  // namespace ash::graduation

#endif  // CHROME_BROWSER_UI_ASH_GRADUATION_GRADUATION_MANAGER_H_
