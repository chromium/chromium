// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_OBSERVER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_OBSERVER_H_

#include <optional>

#include "base/observer_list_types.h"
#include "chromeos/ash/services/secure_channel/connection.h"
#include "chromeos/ash/services/secure_channel/public/mojom/nearby_connector.mojom-shared.h"

namespace ash::secure_channel {

class WireMessage;

// An interface for observing events that happen on a Connection.
class ConnectionObserver : public base::CheckedObserver {
 public:
  // Called when the |connection|'s status changes from |old_status| to
  // |new_status|. The |connectoin| is guaranteed to be non-null.
  virtual void OnConnectionStatusChanged(Connection* connection,
                                         Connection::Status old_status,
                                         Connection::Status new_status) {}

  // Called when a |message| is received from a remote device over the
  // |connection|.
  virtual void OnMessageReceived(const Connection& connection,
                                 const WireMessage& message) {}

  // Called after a |message| is sent to the remote device over the
  // |connection|. |success| is |true| iff the message is sent successfully.
  virtual void OnSendCompleted(const Connection& connection,
                               const WireMessage& message,
                               bool success) {}

 protected:
  ~ConnectionObserver() override = default;
};

class NearbyConnectionObserver : public base::CheckedObserver {
 public:
  virtual void OnNearbyConnectionStateChanged(
      mojom::NearbyConnectionStep step,
      mojom::NearbyConnectionStepResult result) {}

 protected:
  ~NearbyConnectionObserver() override = default;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_CONNECTION_OBSERVER_H_
