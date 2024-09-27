// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_client.h"

#include <algorithm>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/ping_manager.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/task_check_for_update.h"
#include "components/update_client/task_send_ping.h"
#include "components/update_client/task_update.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_client_internal.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

namespace update_client {

CrxInstaller::InstallParams::InstallParams(
    const std::string& run,
    const std::string& arguments,
    const std::string& server_install_data)
    : run(run),
      arguments(arguments),
      server_install_data(server_install_data) {}

CrxUpdateItem::CrxUpdateItem() : state(ComponentState::kNew) {}
CrxUpdateItem::CrxUpdateItem(const CrxUpdateItem& other) = default;
CrxUpdateItem& CrxUpdateItem::operator=(const CrxUpdateItem& other) = default;
CrxUpdateItem::~CrxUpdateItem() = default;

CrxComponent::CrxComponent() = default;
CrxComponent::CrxComponent(const CrxComponent& other) = default;
CrxComponent& CrxComponent::operator=(const CrxComponent& other) = default;
CrxComponent::~CrxComponent() = default;

// It is important that an instance of the UpdateClient binds an unretained
// pointer to itself. Otherwise, a life time circular dependency between this
// instance and its inner members prevents the destruction of this instance.
// Using unretained references is allowed in this case since the life time of
// the UpdateClient instance exceeds the life time of its inner members,
// including any sequences that might execute callbacks bound to it.
UpdateClientImpl::UpdateClientImpl(
    scoped_refptr<Configurator> config,
    scoped_refptr<PingManager> ping_manager,
    UpdateChecker::Factory update_checker_factory)
    : config_(config),
      ping_manager_(ping_manager),
      update_engine_(base::MakeRefCounted<UpdateEngine>(
          config,
          update_checker_factory,
          ping_manager_.get(),
          base::BindRepeating(&UpdateClientImpl::NotifyObservers,
                              base::Unretained(this)))) {}

UpdateClientImpl::~UpdateClientImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(task_queue_.empty());
  CHECK(tasks_.empty());

  config_ = nullptr;
}

base::RepeatingClosure UpdateClientImpl::Install(
    const std::string& id,
    CrxDataCallback crx_data_callback,
    CrxStateChangeCallback crx_state_change_callback,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (IsUpdating(id)) {
    std::move(callback).Run(Error::UPDATE_IN_PROGRESS);
    return base::DoNothing();
  }

  // Install tasks are run concurrently in the foreground and never queued up.
  auto task = base::MakeRefCounted<TaskUpdate>(
      update_engine_.get(), /*is_foreground=*/true, /*is_install=*/true,
      std::vector<std::string>{id}, std::move(crx_data_callback),
      crx_state_change_callback,
      base::BindOnce(&UpdateClientImpl::OnTaskComplete, this,
                     std::move(callback)));
  RunTask(task);
  return base::BindRepeating(&Task::Cancel, task);
}

// Update tasks are background tasks and queued up.
void UpdateClientImpl::Update(const std::vector<std::string>& ids,
                              CrxDataCallback crx_data_callback,
                              CrxStateChangeCallback crx_state_change_callback,
                              bool is_foreground,
                              Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RunOrEnqueueTask(base::MakeRefCounted<TaskUpdate>(
      update_engine_.get(), is_foreground, /*is_install=*/false, ids,
      std::move(crx_data_callback), crx_state_change_callback,
      base::BindOnce(&UpdateClientImpl::OnTaskComplete, this,
                     std::move(callback))));
}

// Update check tasks are queued up.
void UpdateClientImpl::CheckForUpdate(
    const std::string& id,
    CrxDataCallback crx_data_callback,
    CrxStateChangeCallback crx_state_change_callback,
    bool is_foreground,
    Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RunOrEnqueueTask(base::MakeRefCounted<TaskCheckForUpdate>(
      update_engine_.get(), id, std::move(crx_data_callback),
      crx_state_change_callback, is_foreground,
      base::BindOnce(&UpdateClientImpl::OnTaskComplete, this,
                     std::move(callback))));
}

void UpdateClientImpl::RunTask(scoped_refptr<Task> task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&Task::Run, task));
  tasks_.insert(task);
}

void UpdateClientImpl::RunOrEnqueueTask(scoped_refptr<Task> task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (tasks_.empty()) {
    RunTask(task);
  } else {
    task_queue_.push_back(task);
  }
}

void UpdateClientImpl::OnTaskComplete(Callback callback,
                                      scoped_refptr<Task> task,
                                      Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(task);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), error));

  tasks_.erase(task);

  if (is_stopped_) {
    return;
  }

  // Pick up a task from the queue if the queue has pending tasks and no other
  // task is running.
  if (tasks_.empty() && !task_queue_.empty()) {
    auto queued_task = task_queue_.front();
    task_queue_.pop_front();
    RunTask(queued_task);
  }
}

void UpdateClientImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void UpdateClientImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

void UpdateClientImpl::NotifyObservers(const CrxUpdateItem& item) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : observer_list_) {
    observer.OnEvent(item);
  }
}

bool UpdateClientImpl::GetCrxUpdateState(const std::string& id,
                                         CrxUpdateItem* update_item) const {
  return update_engine_->GetUpdateState(id, update_item);
}

bool UpdateClientImpl::IsUpdating(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& task : tasks_) {
    const auto ids = task->GetIds();
    if (base::Contains(ids, id)) {
      return true;
    }
  }

  for (const auto& task : task_queue_) {
    const auto ids = task->GetIds();
    if (base::Contains(ids, id)) {
      return true;
    }
  }

  return false;
}

void UpdateClientImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  is_stopped_ = true;

  // In the current implementation it is sufficient to cancel the pending
  // tasks only. The tasks that are run by the update engine will stop
  // making progress naturally, as the main task runner stops running task
  // actions. Upon the browser shutdown, the resources employed by the active
  // tasks will leak, as the operating system kills the thread associated with
  // the update engine task runner. Further refactoring may be needed in this
  // area, to cancel the running tasks by canceling the current action update.
  // This behavior would be expected, correct, and result in no resource leaks
  // in all cases, in shutdown or not.
  //
  // Cancel the pending tasks. These tasks are safe to cancel and delete since
  // they have not picked up by the update engine, and not shared with any
  // task runner yet.
  while (!task_queue_.empty()) {
    auto task = task_queue_.front();
    task_queue_.pop_front();
    task->Cancel();
  }
}

void UpdateClientImpl::SendPing(const CrxComponent& crx_component,
                                PingParams ping_params,
                                Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RunTask(base::MakeRefCounted<TaskSendPing>(
      update_engine_.get(), crx_component, ping_params,
      base::BindOnce(&UpdateClientImpl::OnTaskComplete, this,
                     std::move(callback))));
}

scoped_refptr<UpdateClient> UpdateClientFactory(
    scoped_refptr<Configurator> config) {
  return base::MakeRefCounted<UpdateClientImpl>(
      config, base::MakeRefCounted<PingManager>(config),
      base::BindRepeating(&UpdateChecker::Create));
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  RegisterPersistedDataPrefs(registry);
}

// This function has the exact same implementation as RegisterPrefs. We have
// this implementation here to make the intention more clear that is local user
// profile access is needed.
void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  RegisterPersistedDataPrefs(registry);
}

}  // namespace update_client
