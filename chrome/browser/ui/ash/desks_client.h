// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DESKS_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_DESKS_CLIENT_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"

namespace ash {
class DesksHelper;
class DeskTemplate;
}  // namespace ash

// Class to handle all Desks in-browser functionalities. Will call into
// ash::DesksController (via ash::DesksHelper) to do actual desk related
// operations.
class DesksClient {
 public:
  DesksClient();
  DesksClient(const DesksClient&) = delete;
  DesksClient& operator=(const DesksClient&) = delete;
  ~DesksClient();

  static DesksClient* Get();

  // Captures the active desk and returns it as a desk template containing
  // necessary information that can be used to recreate a same desk later.
  // Returns a nullptr if no such template can be captured. Note the captured
  // template will not be saved to storage.
  std::unique_ptr<ash::DeskTemplate> CaptureActiveDeskAsTemplate();

  // Launches the desk template with `template_uuid` as a new desk. `desk_uuid`
  // is the unique id for an existing desk template.
  void LaunchDeskTemplate(double template_uuid);

 private:
  friend class DesksClientTest;

  // Callback function that is ran after a desk is created, or has failed to be
  // created.
  void OnCreateAndActivateNewDesk(ash::DeskTemplate* desk_template,
                                  bool on_create_activate_success);

  // Convenience pointer to the desks helper which is `ash::DesksController`.
  // Guaranteed to be not null for the duration of `this`.
  ash::DesksHelper* const desks_helper_;

  // A test only template for testing `LaunchDeskTemplate`.
  std::unique_ptr<ash::DeskTemplate> launch_template_for_test_;

  base::WeakPtrFactory<DesksClient> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_DESKS_CLIENT_H_
