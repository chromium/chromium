// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PHONEHUB_FAKE_CONNECTION_MANAGER_H_
#define CHROMEOS_COMPONENTS_PHONEHUB_FAKE_CONNECTION_MANAGER_H_

#include "chromeos/components/phonehub/connection_manager.h"

namespace chromeos {
namespace phonehub {

class FakeConnectionManager : public ConnectionManager {
 public:
  FakeConnectionManager();
  ~FakeConnectionManager() override;

  void SetStatus(Status status);

 private:
  // ConnectionManager:
  Status GetStatus() const override;
  void AttemptConnection() override;

  Status status_;
};

}  // namespace phonehub
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PHONEHUB_FAKE_CONNECTION_MANAGER_H_
