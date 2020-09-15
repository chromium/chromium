// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_CONNECTION_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_CONNECTION_MANAGER_H_

#include <vector>
#include "chromeos/components/phonehub/connection_manager.h"

namespace chromeos {
namespace phonehub {

class FakeConnectionManager : public ConnectionManager {
 public:
  FakeConnectionManager();
  ~FakeConnectionManager() override;

  using ConnectionManager::NotifyMessageReceived;

  void SetStatus(Status status);
  const std::vector<std::string>& sent_messages() const {
    return sent_messages_;
  }
  size_t num_attempt_connection_calls() const {
    return num_attempt_connection_calls_;
  }

 private:
  // ConnectionManager:
  Status GetStatus() const override;
  void AttemptConnection() override;
  void SendMessage(const std::string& payload) override;

  Status status_;
  std::vector<std::string> sent_messages_;
  size_t num_attempt_connection_calls_ = 0;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_CONNECTION_MANAGER_H_
