// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PING_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PING_MANAGER_H_

namespace ash::phonehub {

// The PingManager is responsible for sending a PingRequest proto to the Phone
// from the MessageSender while also observing when the MessageReceiver
// receives a PingResponse proto from the Phone. If a ping is not responded to,
// the ConnectionManager will tear down the connection.
class PingManager {
 public:
  PingManager(const PingManager&) = delete;
  PingManager* operator=(const PingManager&) = delete;
  virtual ~PingManager() = default;

  virtual void SendPingRequest() = 0;

 private:
  virtual void Reset() = 0;

 protected:
  PingManager() = default;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PING_MANAGER_H_
