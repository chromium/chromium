// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hps/fake_hps_dbus_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

FakeHpsDBusClient::FakeHpsDBusClient() = default;

FakeHpsDBusClient::~FakeHpsDBusClient() = default;

void FakeHpsDBusClient::AddObserver(Observer* observer) {}

void FakeHpsDBusClient::RemoveObserver(Observer* observer) {}

void FakeHpsDBusClient::GetResultHpsNotify(GetResultHpsNotifyCallback cb) {
  std::move(cb).Run(absl::nullopt);
}

}  // namespace chromeos
