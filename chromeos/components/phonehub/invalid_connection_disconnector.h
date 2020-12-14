// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_INVALID_CONNECTION_DISCONNECTOR_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_INVALID_CONNECTION_DISCONNECTOR_H_

#include "chromeos/components/phonehub/connection_manager.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace chromeos {
namespace phonehub {

class PhoneModel;

// Disconnects the phone if the ConnectionManager is in the kConnected
// state, but the PhoneStatusModel remains empty after a grace period.
class InvalidConnectionDisconnector : public ConnectionManager::Observer {
 public:
  InvalidConnectionDisconnector(ConnectionManager* connection_manager,
                                PhoneModel* phone_model);
  ~InvalidConnectionDisconnector() override;

  InvalidConnectionDisconnector(const InvalidConnectionDisconnector&) = delete;
  InvalidConnectionDisconnector* operator=(
      const InvalidConnectionDisconnector&) = delete;

 private:
  friend class InvalidConnectionDisconnectorTest;

  InvalidConnectionDisconnector(ConnectionManager* connection_manager,
                                PhoneModel* phone_model,
                                std::unique_ptr<base::OneShotTimer> timer);

  // ConnectionManager::Observer:
  void OnConnectionStatusChanged() override;

  void UpdateTimer();
  void OnTimerFired();

  bool IsPhoneConnected() const;
  bool DoesPhoneStatusModelExist() const;

  ConnectionManager* connection_manager_;
  PhoneModel* phone_model_;
  std::unique_ptr<base::OneShotTimer> timer_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_INVALID_CONNECTION_DISCONNECTOR_H_
