// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/internal/command_internal.h"

#include <string_view>
#include <type_traits>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/values.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"

namespace web_app::internal {
namespace {
int GetNextCommandId() {
  static base::AtomicSequenceNumber g_incrementing_id_;
  return g_incrementing_id_.GetNext();
}
}  // namespace

CommandBase::CommandBase(std::string name)
    : id_(GetNextCommandId()), name_(std::move(name)) {
  DETACH_FROM_SEQUENCE(command_sequence_checker_);
  // We don't have an easy way to enforce construction of class on the
  // WebAppProvider sequence without requiring a UI thread in unittests, so just
  // allow this construction to happen from any thread.

  base::Value::Dict* metadata = GetMutableDebugValue().EnsureDict("!metadata");
  metadata->Set("name", name_);
  metadata->Set("id", id_);
  metadata->Set("started", false);
}
CommandBase::~CommandBase() = default;

content::WebContents* CommandBase::GetInstallingWebContents(
    base::PassKey<WebAppCommandManager>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(command_sequence_checker_);
  return nullptr;
}

void CommandBase::OnShutdown(base::PassKey<WebAppCommandManager>) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(command_sequence_checker_);
}

bool CommandBase::IsStarted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(command_sequence_checker_);
  return started_;
}

base::WeakPtr<CommandBase> CommandBase::GetBaseCommandWeakPtr() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(command_sequence_checker_);
  return weak_factory_.GetWeakPtr();
}

void CommandBase::SetScheduledLocation(base::PassKey<WebAppCommandManager>,
                                       const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(command_sequence_checker_);
  GetMutableDebugValue()
      .EnsureDict("!metadata")
      ->Set("scheduled_location", location.ToString());
}

void CommandBase::SetCommandManager(base::PassKey<WebAppCommandManager>,
                                    WebAppCommandManager* command_manager) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(command_sequence_checker_);
  command_manager_ = command_manager;
}

const base::Value::Dict& CommandBase::GetDebugValue() const {
  return debug_value_;
}

WebAppCommandManager* CommandBase::command_manager() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(command_sequence_checker_);
  return command_manager_;
}

base::Value::Dict& CommandBase::GetMutableDebugValue() {
  return debug_value_;
}

void CommandBase::SetStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(command_sequence_checker_);
  started_ = true;
  GetMutableDebugValue().EnsureDict("!metadata")->Set("started", true);
}

void CommandBase::CompleteAndSelfDestructInternal(
    CommandResult result,
    base::OnceClosure after_destruction) {
  command_manager()->OnCommandComplete(base::PassKey<CommandBase>(), this,
                                       result, std::move(after_destruction));
}

template <typename LockType>
CommandWithLock<LockType>::CommandWithLock(const std::string& name,
                                           LockDescription initial_lock_request)
    : CommandBase(name),
      initial_lock_request_(std::move(initial_lock_request)),
      initial_lock_(std::make_unique<LockType>()) {
  GetMutableDebugValue()
      .EnsureDict("!metadata")
      ->Set("initial_lock_request", initial_lock_request_.AsDebugValue());
}

template <typename LockType>
CommandWithLock<LockType>::~CommandWithLock() = default;

template <typename LockType>
void CommandWithLock<LockType>::RequestLock(
    base::PassKey<WebAppCommandManager>,
    WebAppLockManager* lock_manager,
    LockAcquiredCallback on_lock_acquired,
    const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(command_sequence_checker_);
  lock_manager->AcquireLock(
      initial_lock_request_, *initial_lock_,
      base::BindOnce(&CommandWithLock::PrepareForStart,
                     weak_factory_.GetWeakPtr(), std::move(on_lock_acquired)),
      location);
}

template <typename LockType>
void CommandWithLock<LockType>::PrepareForStart(
    LockAcquiredCallback on_lock_acquired) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(command_sequence_checker_);
  CHECK(command_manager());
  CHECK(!IsStarted());
  SetStarted();
  std::move(on_lock_acquired)
      .Run(base::BindOnce(&CommandWithLock::StartWithLock,
                          weak_factory_.GetWeakPtr(),
                          std::move(initial_lock_)));
}

template <typename LockType>
bool CommandWithLock<LockType>::ShouldPrepareWebContentsBeforeStart(
    base::PassKey<WebAppCommandManager>) const {
  return initial_lock_request_.IncludesSharedWebContents();
}

template class CommandWithLock<NoopLock>;
template class CommandWithLock<SharedWebContentsLock>;
template class CommandWithLock<AppLock>;
template class CommandWithLock<SharedWebContentsWithAppLock>;
template class CommandWithLock<AllAppsLock>;

}  // namespace web_app::internal
