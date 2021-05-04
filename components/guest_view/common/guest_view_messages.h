// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for GuestViews.

#ifndef COMPONENTS_GUEST_VIEW_COMMON_GUEST_VIEW_MESSAGES_H_
#define COMPONENTS_GUEST_VIEW_COMMON_GUEST_VIEW_MESSAGES_H_

#include "base/values.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"

#define IPC_MESSAGE_START GuestViewMsgStart

// Messages sent from the browser to the renderer.

// Tells the embedder that a guest has been attached in --site-per-process mode.
IPC_MESSAGE_CONTROL1(GuestViewMsg_AttachToEmbedderFrame_ACK,
                     int /* element_instance_id */)

// Messages sent from the renderer to the browser.

// We have a RenderFrame with routing id of |embedder_local_frame_routing_id|.
// We want this local frame to be replaced with a remote frame that points
// to a GuestView. This message will attach the local frame to the guest.
// The GuestView is identified by its ID: |guest_instance_id|.
IPC_MESSAGE_CONTROL4(GuestViewHostMsg_AttachToEmbedderFrame,
                     int /* embedder_local_frame_routing_id */,
                     int /* element_instance_id */,
                     int /* guest_instance_id */,
                     base::DictionaryValue /* params */)

// Sent by the renderer when a GuestView (identified by |view_instance_id|) has
// been created in JavaScript.
IPC_MESSAGE_CONTROL2(GuestViewHostMsg_ViewCreated,
                     int /* view_instance_id */,
                     std::string /* view_type */)

// Sent by the renderer when a GuestView (identified by |view_instance_id|) has
// been garbage collected in JavaScript.
IPC_MESSAGE_CONTROL1(GuestViewHostMsg_ViewGarbageCollected,
                     int /* view_instance_id */)

#endif  // COMPONENTS_GUEST_VIEW_COMMON_GUEST_VIEW_MESSAGES_H_
