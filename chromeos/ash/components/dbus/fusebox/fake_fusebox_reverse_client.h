// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_FUSEBOX_FAKE_FUSEBOX_REVERSE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_FUSEBOX_FAKE_FUSEBOX_REVERSE_CLIENT_H_

#include "chromeos/ash/components/dbus/fusebox/fusebox_reverse_client.h"

namespace ash {

class FakeFuseBoxReverseClient : public FuseBoxReverseClient {
 public:
  FakeFuseBoxReverseClient();

  FakeFuseBoxReverseClient(const FakeFuseBoxReverseClient&) = delete;
  FakeFuseBoxReverseClient& operator=(const FakeFuseBoxReverseClient&) = delete;

  ~FakeFuseBoxReverseClient() override;

  void AttachStorage(const std::string& name, StorageResult callback) override;

  void DetachStorage(const std::string& name, StorageResult callback) override;

  void ReplyToReadDir(uint64_t handle,
                      int32_t error_code,
                      fusebox::DirEntryListProto dir_entry_list_proto,
                      bool has_more) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_FUSEBOX_FAKE_FUSEBOX_REVERSE_CLIENT_H_
