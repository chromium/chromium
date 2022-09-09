// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_command.h"

#include "base/atomic_sequence_num.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "components/services/storage/indexed_db/locks/leveled_lock_manager.h"
#include "components/services/storage/indexed_db/locks/leveled_lock_range.h"

namespace web_app {

WebAppCommand::WebAppCommand() {
  DETACH_FROM_SEQUENCE(command_sequence_checker_);
  // We don't have an easy way to enforce construction of class on the
  // WebAppProvider sequence without requiring a UI thread in unittests, so just
  // allow this construction to happen from any thread.
  static base::AtomicSequenceNumber g_incrementing_id_;
  id_ = g_incrementing_id_.GetNext();
}
WebAppCommand::~WebAppCommand() = default;

content::WebContents* WebAppCommand::GetInstallingWebContents() {
  return nullptr;
}

void WebAppCommand::SignalCompletionAndSelfDestruct(
    CommandResult result,
    base::OnceClosure completion_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(command_sequence_checker_);
  // Surround the check in an if-statement to avoid evaluating the debug value
  // every time.
  if (!command_manager_) {
    CHECK(command_manager_)
        << "Command was never started: " << ToDebugValue().DebugString();
  }
  command_manager_->OnCommandComplete(this, result,
                                      std::move(completion_callback));
}

void WebAppCommand::Start(WebAppCommandManager* command_manager) {
  command_manager_ = command_manager;
  Start();
}

base::WeakPtr<WebAppCommand> WebAppCommand::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace web_app
