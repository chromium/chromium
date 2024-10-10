// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/messaging/message.h"

namespace tab_groups::messaging {

MessageAttribution::MessageAttribution() = default;
MessageAttribution::MessageAttribution(const MessageAttribution& other) =
    default;
MessageAttribution::~MessageAttribution() = default;

TabGroupMessageMetadata::TabGroupMessageMetadata() = default;
TabGroupMessageMetadata::TabGroupMessageMetadata(
    const TabGroupMessageMetadata& other) = default;
TabGroupMessageMetadata::~TabGroupMessageMetadata() = default;

TabMessageMetadata::TabMessageMetadata() = default;
TabMessageMetadata::TabMessageMetadata(const TabMessageMetadata& other) =
    default;
TabMessageMetadata::~TabMessageMetadata() = default;

}  // namespace tab_groups::messaging
