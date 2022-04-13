// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/callback_command.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"

namespace web_app {

CallbackCommand::CallbackCommand(WebAppCommandQueueId queue_id,
                                 base::OnceClosure callback)
    : WebAppCommand(queue_id), callback_(std::move(callback)) {}

CallbackCommand::~CallbackCommand() = default;

void CallbackCommand::Start() {
  return SignalCompletionAndSelfDestruct(
      CommandResult::kSuccess, base::BindOnce(std::move(callback_)), {});
}

base::Value CallbackCommand::ToDebugValue() const {
  return base::Value(base::StringPrintf("CallbackCommand %d", id()));
}
}  // namespace web_app
