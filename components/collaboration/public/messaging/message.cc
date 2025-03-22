// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/messaging/message.h"

namespace collaboration::messaging {

MessageAttribution::MessageAttribution() = default;
MessageAttribution::MessageAttribution(const MessageAttribution& other) =
    default;
MessageAttribution::~MessageAttribution() = default;

AggregatedMessageData::AggregatedMessageData() = default;
AggregatedMessageData::AggregatedMessageData(
    const AggregatedMessageData& other) = default;
AggregatedMessageData::~AggregatedMessageData() = default;

InstantMessage::InstantMessage() = default;
InstantMessage::InstantMessage(const InstantMessage& other) = default;
InstantMessage::~InstantMessage() = default;

TabGroupMessageMetadata::TabGroupMessageMetadata() = default;
TabGroupMessageMetadata::TabGroupMessageMetadata(
    const TabGroupMessageMetadata& other) = default;
TabGroupMessageMetadata::~TabGroupMessageMetadata() = default;

TabMessageMetadata::TabMessageMetadata() = default;
TabMessageMetadata::TabMessageMetadata(const TabMessageMetadata& other) =
    default;
TabMessageMetadata::~TabMessageMetadata() = default;

}  // namespace collaboration::messaging
