// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_APP_MANAGER_H_
#define CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_APP_MANAGER_H_

#include <stdint.h>

#include "chromeos/components/eche_app_ui/eche_notification_click_handler.h"
#include "chromeos/components/phonehub/phone_hub_manager.h"
#include "components/keyed_service/core/keyed_service.h"

namespace chromeos {
namespace eche_app {

// Implements the core logic of the EcheApp and exposes interfaces via its
// public API. Implemented as a KeyedService since it depends on other
// KeyedService instances.
class EcheAppManager : public KeyedService {
 public:
  EcheAppManager(phonehub::PhoneHubManager*,
                 EcheNotificationClickHandler::LaunchEcheAppFunction);
  ~EcheAppManager() override;

  EcheAppManager(const EcheAppManager&) = delete;
  EcheAppManager& operator=(const EcheAppManager&) = delete;

  // KeyedService:
  void Shutdown() override;

 private:
  std::unique_ptr<EcheNotificationClickHandler>
      eche_notification_click_handler_;
};

}  // namespace eche_app
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_ECHE_APP_UI_ECHE_APP_MANAGER_H_
