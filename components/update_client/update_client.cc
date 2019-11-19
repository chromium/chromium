// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/update_client.h"

#include <algorithm>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/stl_util.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/crx_file/crx_verifier.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/ping_manager.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/task_send_uninstall_ping.h"
#include "components/update_client/task_update.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/update_client_internal.h"
#include "components/update_client/update_engine.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

namespace update_client {

CrxUpdateItem::CrxUpdateItem() : state(ComponentState::kNew) {}
CrxUpdateItem::~CrxUpdateItem() = default;
CrxUpdateItem::CrxUpdateItem(const CrxUpdateItem& other) = default;

CrxComponent::CrxComponent()
    : allows_background_download(true),
      requires_network_encryption(true),
      crx_format_requirement(
          crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF),
      supports_group_policy_enable_component_updates(false) {}
CrxComponent::CrxComponent(const CrxComponent& other) = default;
CrxComponent::~CrxComponent() = default;

// It is important that an instance of the UpdateClient binds an unretained
// pointer to itself. Otherwise, a life time circular dependency between this
// instance and its inner members prevents the destruction of this instance.
// Using unretained references is allowed in this case since the life time of
// the UpdateClient instance exceeds the life time of its inner members,
// including any thread objects that might execute callbacks bound to it.
UpdateClientImpl::UpdateClientImpl(
    scoped_refptr<Configurator> config,
    scoped_refptr<PingManager> ping_manager,
    UpdateChecker::Factory update_checker_factory,
    CrxDownloader::Factory crx_downloader_factory)
    : is_stopped_(false),
      config_(config),
      ping_manager_(ping_manager),
      update_engine_(base::MakeRefCounted<UpdateEngine>(
          config,
          update_checker_factory,
          crx_downloader_factory,
          ping_manager_.get(),
          base::Bind(&UpdateClientImpl::NotifyObservers,
                     base::Unretained(this)))) {}

UpdateClientImpl::~UpdateClientImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(task_queue_.empty());
  DCHECK(tasks_.empty());

  config_ = nullptr;
}

void UpdateClientImpl::Install(const std::string& id,
                               CrxDataCallback crx_data_callback,
                               Callback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (IsUpdating(id)) {
    std::move(callback).Run(Error::UPDATE_IN_PROGRESS);
    return;
  }

  std::vector<std::string> ids = {id};

  // Install tasks are run concurrently and never queued up. They are always
  // considered foreground tasks.
  constexpr bool kIsForeground = true;
  RunTask(base::MakeRefCounted<TaskUpdate>(
      update_engine_.get(), kIsForeground, ids, std::move(crx_data_callback),
      base::BindOnce(&UpdateClientImpl::OnTaskComplete, this,
                     std::move(callback))));
}

void UpdateClientImpl::Update(const std::vector<std::string>& ids,
                              CrxDataCallback crx_data_callback,
                              bool is_foreground,
                              Callback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto task = base::MakeRefCounted<TaskUpdate>(
      update_engine_.get(), is_foreground, ids, std::move(crx_data_callback),
      base::BindOnce(&UpdateClientImpl::OnTaskComplete, this,
                     std::move(callback)));

  // If no other tasks are running at the moment, run this update task.
  // Otherwise, queue the task up.
  if (tasks_.empty()) {
    RunTask(task);
  } else {
    task_queue_.push_back(task);
  }
}

void UpdateClientImpl::RunTask(scoped_refptr<Task> task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&Task::Run, base::Unretained(task.get())));
  tasks_.insert(task);
}

void UpdateClientImpl::OnTaskComplete(Callback callback,
                                      scoped_refptr<Task> task,
                                      Error error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(task);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), error));

  // Remove the task from the set of the running tasks. Only tasks handled by
  // the update engine can be in this data structure.
  tasks_.erase(task);

  if (is_stopped_)
    return;

  // Pick up a task from the queue if the queue has pending tasks and no other
  // task is running.
  if (tasks_.empty() && !task_queue_.empty()) {
    auto task = task_queue_.front();
    task_queue_.pop_front();
    RunTask(task);
  }
}

void UpdateClientImpl::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.AddObserver(observer);
}

void UpdateClientImpl::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  observer_list_.RemoveObserver(observer);
}

void UpdateClientImpl::NotifyObservers(Observer::Events event,
                                       const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& observer : observer_list_)
    observer.OnEvent(event, id);
}

bool UpdateClientImpl::GetCrxUpdateState(const std::string& id,
                                         CrxUpdateItem* update_item) const {
  return update_engine_->GetUpdateState(id, update_item);
}

bool UpdateClientImpl::IsUpdating(const std::string& id) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (const auto task : tasks_) {
    const auto ids = task->GetIds();
    if (base::Contains(ids, id)) {
      return true;
    }
  }

  for (const auto task : task_queue_) {
    const auto ids = task->GetIds();
    if (base::Contains(ids, id)) {
      return true;
    }
  }

  return false;
}

void UpdateClientImpl::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

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

void UpdateClientImpl::SendUninstallPing(const std::string& id,
                                         const base::Version& version,
                                         int reason,
                                         Callback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  RunTask(base::MakeRefCounted<TaskSendUninstallPing>(
      update_engine_.get(), id, version, reason,
      base::BindOnce(&UpdateClientImpl::OnTaskComplete, base::Unretained(this),
                     std::move(callback))));
}

scoped_refptr<UpdateClient> UpdateClientFactory(
    scoped_refptr<Configurator> config) {
  return base::MakeRefCounted<UpdateClientImpl>(
      config, base::MakeRefCounted<PingManager>(config), &UpdateChecker::Create,
      &CrxDownloader::Create);
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  PersistedData::RegisterPrefs(registry);
}

// This function has the exact same implementation as RegisterPrefs. We have
// this implementation here to make the intention more clear that is local user
// profile access is needed.
void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  PersistedData::RegisterPrefs(registry);
}

}  // namespace update_client
