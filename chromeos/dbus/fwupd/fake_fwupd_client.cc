// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fwupd/fake_fwupd_client.h"

namespace chromeos {

FakeFwupdClient::FakeFwupdClient() = default;
FakeFwupdClient::~FakeFwupdClient() = default;
void FakeFwupdClient::Init(dbus::Bus* bus) {}

}  // namespace chromeos