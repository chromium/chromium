// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_DISCONNECT_TETHERING_REQUEST_SENDER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_DISCONNECT_TETHERING_REQUEST_SENDER_H_

#include "base/observer_list.h"

namespace ash {

namespace tether {

// Sends a DisconnectTetheringRequest to the formerly active host. Supports
// multiple concurrent messages.
class DisconnectTetheringRequestSender {
 public:
  class Observer {
   public:
    Observer() {}
    virtual ~Observer() {}

    virtual void OnPendingDisconnectRequestsComplete() {}
  };

  DisconnectTetheringRequestSender();

  DisconnectTetheringRequestSender(const DisconnectTetheringRequestSender&) =
      delete;
  DisconnectTetheringRequestSender& operator=(
      const DisconnectTetheringRequestSender&) = delete;

  virtual ~DisconnectTetheringRequestSender();

  // Sends a DisconnectTetheringRequest to the device with the given ID.
  virtual void SendDisconnectRequestToDevice(const std::string& device_id) = 0;

  // Returns whether at least one DisconnectTetheringRequest is still in the
  // process of being sent.
  virtual bool HasPendingRequests() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyPendingDisconnectRequestsComplete();

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_DISCONNECT_TETHERING_REQUEST_SENDER_H_
