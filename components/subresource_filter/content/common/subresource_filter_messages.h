// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Message definition file, included multiple times, hence no include guard.
// no-include-guard-because-multiply-included

#include "base/time/time.h"
#include "content/public/common/common_param_traits_macros.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_platform_file.h"

#define IPC_MESSAGE_START SubresourceFilterMsgStart

// ----------------------------------------------------------------------------
// Messages sent from the browser to the renderer.
// ----------------------------------------------------------------------------

// Sends a read-only mode file handle with the ruleset data to a renderer
// process, containing the subresource filtering rules to be consulted for all
// subsequent document loads that have subresource filtering activated.
IPC_MESSAGE_CONTROL1(SubresourceFilterMsg_SetRulesetForProcess,
                     IPC::PlatformFileForTransit /* ruleset_file */)
