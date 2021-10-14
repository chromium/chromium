// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/spaced/fake_spaced_client.h"

namespace chromeos {

FakeSpacedClient::FakeSpacedClient() = default;

FakeSpacedClient::~FakeSpacedClient() = default;

void FakeSpacedClient::GetFreeDiskSpace(const std::string& path,
                                        GetSizeCallback callback) {
  std::move(callback).Run(absl::nullopt);
}

void FakeSpacedClient::GetTotalDiskSpace(const std::string& path,
                                         GetSizeCallback callback) {
  std::move(callback).Run(absl::nullopt);
}

void FakeSpacedClient::GetRootDeviceSize(GetSizeCallback callback) {
  std::move(callback).Run(absl::nullopt);
}

}  // namespace chromeos
