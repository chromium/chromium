// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/callback_command.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/web_applications/locks/lock.h"

namespace web_app {

CallbackCommand::CallbackCommand(std::unique_ptr<Lock> lock,
                                 base::OnceClosure callback)
    : lock_(std::move(lock)), callback_(std::move(callback)) {
  DCHECK(lock_);
}

CallbackCommand::~CallbackCommand() = default;

void CallbackCommand::Start() {
  return SignalCompletionAndSelfDestruct(CommandResult::kSuccess,
                                         base::BindOnce(std::move(callback_)));
}

Lock& CallbackCommand::lock() const {
  return *lock_;
}

base::Value CallbackCommand::ToDebugValue() const {
  return base::Value(base::StringPrintf("CallbackCommand %d", id()));
}
}  // namespace web_app
