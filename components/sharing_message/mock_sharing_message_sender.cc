// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/mock_sharing_message_sender.h"

MockSharingMessageSender::MockSharingMessageSender()
    : SharingMessageSender(
          /*local_device_info_provider=*/nullptr,
          /*task_ruunner=*/nullptr) {}

MockSharingMessageSender::~MockSharingMessageSender() = default;
