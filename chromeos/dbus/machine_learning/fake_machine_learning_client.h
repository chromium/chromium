// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MACHINE_LEARNING_FAKE_MACHINE_LEARNING_CLIENT_H_
#define CHROMEOS_DBUS_MACHINE_LEARNING_FAKE_MACHINE_LEARNING_CLIENT_H_

#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"

namespace chromeos {

// Fake implementation of MachineLearningClient. This is currently a no-op fake.
class FakeMachineLearningClient : public MachineLearningClient {
 public:
  FakeMachineLearningClient();

  FakeMachineLearningClient(const FakeMachineLearningClient&) = delete;
  FakeMachineLearningClient& operator=(const FakeMachineLearningClient&) =
      delete;

  ~FakeMachineLearningClient() override;

  // MachineLearningClient:
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> result_callback) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MACHINE_LEARNING_FAKE_MACHINE_LEARNING_CLIENT_H_
