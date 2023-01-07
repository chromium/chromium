// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"

namespace ash {

namespace {

FakeSpacedClient* g_instance = nullptr;

}  // namespace

// static
FakeSpacedClient* FakeSpacedClient::Get() {
  return g_instance;
}

FakeSpacedClient::FakeSpacedClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeSpacedClient::~FakeSpacedClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void FakeSpacedClient::GetFreeDiskSpace(const std::string& path,
                                        GetSizeCallback callback) {
  std::move(callback).Run(free_disk_space_);
}

void FakeSpacedClient::GetTotalDiskSpace(const std::string& path,
                                         GetSizeCallback callback) {
  std::move(callback).Run(total_disk_space_);
}

void FakeSpacedClient::GetRootDeviceSize(GetSizeCallback callback) {
  std::move(callback).Run(root_device_size_);
}

}  // namespace ash
