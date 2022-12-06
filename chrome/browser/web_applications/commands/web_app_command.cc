// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_command.h"

#include "base/atomic_sequence_num.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/full_system_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"

namespace web_app {

WebAppCommand::WebAppCommand(const std::string& name) : name_(name) {
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
  if (!command_manager()) {
    CHECK(command_manager())
        << "Command was never started: " << ToDebugValue().DebugString();
  }
  command_manager()->OnCommandComplete(this, result,
                                       std::move(completion_callback));
}

WebAppCommandManager* WebAppCommand::command_manager() const {
  return command_manager_;
}

base::WeakPtr<WebAppCommand> WebAppCommand::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

template <typename LockType>
WebAppCommandTemplate<LockType>::WebAppCommandTemplate(const std::string& name)
    : WebAppCommand(name) {}

template <typename LockType>
WebAppCommandTemplate<LockType>::~WebAppCommandTemplate() = default;

template <typename LockType>
void WebAppCommandTemplate<LockType>::RequestLock(
    WebAppCommandManager* command_manager,
    WebAppLockManager* lock_manager,
    LockAcquiredCallback on_lock_acquired) {
  lock_manager->AcquireLock(
      lock_description(),
      base::BindOnce(&WebAppCommandTemplate::PrepareForStart,
                     weak_factory_.GetWeakPtr(), command_manager,
                     std::move(on_lock_acquired)));
}

template <typename LockType>
void WebAppCommandTemplate<LockType>::PrepareForStart(
    WebAppCommandManager* command_manager,
    LockAcquiredCallback on_lock_acquired,
    std::unique_ptr<LockType> lock) {
  command_manager_ = command_manager;

  std::move(on_lock_acquired)
      .Run(base::BindOnce(&WebAppCommandTemplate::StartWithLock,
                          weak_factory_.GetWeakPtr(), std::move(lock)));
}

template class WebAppCommandTemplate<NoopLock>;
template class WebAppCommandTemplate<SharedWebContentsLock>;
template class WebAppCommandTemplate<AppLock>;
template class WebAppCommandTemplate<SharedWebContentsWithAppLock>;
template class WebAppCommandTemplate<FullSystemLock>;

}  // namespace web_app
