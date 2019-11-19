// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MACHINE_LEARNING_FAKE_MACHINE_LEARNING_CLIENT_H_
#define CHROMEOS_DBUS_MACHINE_LEARNING_FAKE_MACHINE_LEARNING_CLIENT_H_

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/dbus/machine_learning/machine_learning_client.h"

namespace chromeos {

// Fake implementation of MachineLearningClient. This is currently a no-op fake.
class FakeMachineLearningClient : public MachineLearningClient {
 public:
  FakeMachineLearningClient();
  ~FakeMachineLearningClient() override;

  // MachineLearningClient:
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> result_callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeMachineLearningClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MACHINE_LEARNING_FAKE_MACHINE_LEARNING_CLIENT_H_
