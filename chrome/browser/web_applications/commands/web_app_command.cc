// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_command.h"

#include "base/atomic_sequence_num.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"

namespace web_app {

WebAppCommand::WebAppCommand(WebAppCommandQueueId queue_id)
    : queue_id_(std::move(queue_id)) {
  DETACH_FROM_SEQUENCE(command_sequence_checker_);
  // We don't have an easy way to enforce construction of class on the
  // WebAppProvider sequence without requiring a UI thread in unittests, so just
  // allow this construction to happen from any thread.
  static base::AtomicSequenceNumber g_incrementing_id_;
  id_ = g_incrementing_id_.GetNext();
}
WebAppCommand::~WebAppCommand() = default;

void WebAppCommand::SignalCompletionAndSelfDestruct(
    CommandResult result,
    base::OnceClosure completion_callback,
    std::vector<std::unique_ptr<WebAppCommand>> chained_commands) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(command_sequence_checker_);
  // Surround the check in an if-statement to avoid evaluating the debug value
  // every time.
  if (!command_manager_) {
    CHECK(command_manager_)
        << "Command was never started: " << ToDebugValue().DebugString();
  }
  command_manager_->OnCommandComplete(this, result,
                                      std::move(completion_callback),
                                      std::move(chained_commands));
}

void WebAppCommand::Start(WebAppCommandManager* command_manager) {
  command_manager_ = command_manager;
  Start();
}

base::WeakPtr<WebAppCommand> WebAppCommand::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace web_app
