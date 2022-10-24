// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/callback_command.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

CallbackCommand::CallbackCommand(
    std::unique_ptr<LockDescription> lock_description,
    base::OnceClosure callback)
    : lock_description_(std::move(lock_description)),
      callback_(std::move(callback)) {
  DCHECK(lock_description_);
}

CallbackCommand::~CallbackCommand() = default;

void CallbackCommand::Start() {
  return SignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                         base::BindOnce(std::move(callback_)));
}

LockDescription& CallbackCommand::lock_description() const {
  return *lock_description_;
}

base::Value CallbackCommand::ToDebugValue() const {
  return base::Value(base::StringPrintf("CallbackCommand %d", id()));
}
}  // namespace web_app
