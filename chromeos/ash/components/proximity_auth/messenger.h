// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_MESSENGER_H_
#define CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_MESSENGER_H_

namespace ash {
namespace secure_channel {
class ClientChannel;
}
}  // namespace ash

namespace proximity_auth {

class MessengerObserver;

// A messenger handling the Easy Unlock protocol, capable of parsing events from
// the remote device and sending events for the local device.
class Messenger {
 public:
  virtual ~Messenger() {}

  // Adds or removes an observer for Messenger events.
  virtual void AddObserver(MessengerObserver* observer) = 0;
  virtual void RemoveObserver(MessengerObserver* observer) = 0;

  // Sends an unlock event message to the remote device.
  virtual void DispatchUnlockEvent() = 0;

  // Sends a simple request to the remote device to unlock the screen.
  // OnUnlockResponse is called for each observer when the response is returned.
  virtual void RequestUnlock() = 0;

  virtual ash::secure_channel::ClientChannel* GetChannel() const = 0;
};

}  // namespace proximity_auth

#endif  // CHROMEOS_ASH_COMPONENTS_PROXIMITY_AUTH_MESSENGER_H_
