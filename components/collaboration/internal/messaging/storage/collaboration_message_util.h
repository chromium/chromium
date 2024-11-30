// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_COLLABORATION_MESSAGE_UTIL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_COLLABORATION_MESSAGE_UTIL_H_

#include "components/collaboration/internal/messaging/storage/messaging_backend_store.h"
#include "components/collaboration/internal/messaging/storage/protocol/message.pb.h"

namespace collaboration::messaging {

enum class MessageCategory {
  kUnknown,
  kTab,
  kTabGroup,
  kCollaboration,
};

MessageCategory GetMessageCategory(const collaboration_pb::Message& message);

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_STORAGE_COLLABORATION_MESSAGE_UTIL_H_
