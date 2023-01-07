// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/fusebox/fake_fusebox_reverse_client.h"

namespace ash {

FakeFuseBoxReverseClient::FakeFuseBoxReverseClient() = default;

FakeFuseBoxReverseClient::~FakeFuseBoxReverseClient() = default;

void FakeFuseBoxReverseClient::AttachStorage(const std::string& name,
                                             StorageResult callback) {}

void FakeFuseBoxReverseClient::DetachStorage(const std::string& name,
                                             StorageResult callback) {}

void FakeFuseBoxReverseClient::ReplyToReadDir(
    uint64_t handle,
    int32_t error_code,
    fusebox::DirEntryListProto dir_entry_list_proto,
    bool has_more) {}

}  // namespace ash
