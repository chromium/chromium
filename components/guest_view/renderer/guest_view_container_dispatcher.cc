// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/renderer/guest_view_container_dispatcher.h"

#include "components/guest_view/common/guest_view_constants.h"
#include "components/guest_view/renderer/guest_view_container.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"

namespace guest_view {

GuestViewContainerDispatcher::GuestViewContainerDispatcher() {
}

GuestViewContainerDispatcher::~GuestViewContainerDispatcher() {
}

bool GuestViewContainerDispatcher::HandlesMessage(const IPC::Message& message) {
  return IPC_MESSAGE_CLASS(message) == GuestViewMsgStart;
}

bool GuestViewContainerDispatcher::OnControlMessageReceived(
    const IPC::Message& message) {
  if (!HandlesMessage(message))
    return false;

  int element_instance_id = kInstanceIDNone;
  base::PickleIterator iter(message);
  bool success = iter.ReadInt(&element_instance_id);
  DCHECK(success);

  auto* container = GuestViewContainer::FromID(element_instance_id);
  if (!container)
    return false;

  return container->OnMessageReceived(message);
}

}  // namespace guest_view
