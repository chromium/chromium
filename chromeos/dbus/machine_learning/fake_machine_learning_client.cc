// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/machine_learning/fake_machine_learning_client.h"

#include "base/functional/callback.h"

namespace chromeos {

FakeMachineLearningClient::FakeMachineLearningClient() = default;

FakeMachineLearningClient::~FakeMachineLearningClient() = default;

void FakeMachineLearningClient::BootstrapMojoConnection(
    base::ScopedFD fd,
    base::OnceCallback<void(bool success)> result_callback) {
  const bool success = true;
  std::move(result_callback).Run(success);
}

}  // namespace chromeos
