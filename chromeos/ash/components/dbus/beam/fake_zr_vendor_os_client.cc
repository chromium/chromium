// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/beam/fake_zr_vendor_os_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

FakeZrVendorOsClient::FakeZrVendorOsClient() = default;

FakeZrVendorOsClient::~FakeZrVendorOsClient() = default;

void FakeZrVendorOsClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  // Instantly return true (service available) in test environments.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeZrVendorOsClient::Init(dbus::Bus* bus) {}

}  // namespace ash
