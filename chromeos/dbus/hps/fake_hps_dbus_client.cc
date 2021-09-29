// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hps/fake_hps_dbus_client.h"

namespace chromeos {

FakeHpsDBusClient::FakeHpsDBusClient() = default;

FakeHpsDBusClient::~FakeHpsDBusClient() = default;

void FakeHpsDBusClient::AddObserver(Observer* observer) {}

void FakeHpsDBusClient::RemoveObserver(Observer* observer) {}

}  // namespace chromeos
