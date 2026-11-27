// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/mock_sharing_message_sender.h"

#include "base/functional/callback_helpers.h"
#include "components/sharing_message/sharing_channel_sender.h"

MockSharingMessageSender::MockSharingMessageSender()
    : SharingMessageSender(
          /*channel_sender=*/nullptr,
          /*local_device_info_provider=*/nullptr,
          /*sync_service=*/nullptr,
          /*start_sync_flare=*/base::DoNothing(),
          /*task_runner=*/nullptr) {}

MockSharingMessageSender::~MockSharingMessageSender() = default;
