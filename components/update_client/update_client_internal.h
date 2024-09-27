// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_INTERNAL_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_INTERNAL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/update_checker.h"
#include "components/update_client/update_client.h"

namespace update_client {

class Configurator;
class PingManager;
class Task;
class UpdateEngine;
enum class Error;

class UpdateClientImpl : public UpdateClient {
 public:
  UpdateClientImpl(scoped_refptr<Configurator> config,
                   scoped_refptr<PingManager> ping_manager,
                   UpdateChecker::Factory update_checker_factory);

  UpdateClientImpl(const UpdateClientImpl&) = delete;
  UpdateClientImpl& operator=(const UpdateClientImpl&) = delete;

  // Overrides for UpdateClient.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  base::RepeatingClosure Install(
      const std::string& id,
      CrxDataCallback crx_data_callback,
      CrxStateChangeCallback crx_state_change_callback,
      Callback callback) override;
  void CheckForUpdate(const std::string& id,
                      CrxDataCallback crx_data_callback,
                      CrxStateChangeCallback crx_state_change_callback,
                      bool is_foreground,
                      Callback callback) override;
  void Update(const std::vector<std::string>& ids,
              CrxDataCallback crx_data_callback,
              CrxStateChangeCallback crx_state_change_callback,
              bool is_foreground,
              Callback callback) override;
  bool GetCrxUpdateState(const std::string& id,
                         CrxUpdateItem* update_item) const override;
  bool IsUpdating(const std::string& id) const override;
  void Stop() override;
  void SendPing(const CrxComponent& crx_component,
                PingParams ping_params,
                Callback callback) override;

 private:
  ~UpdateClientImpl() override;

  void RunTask(scoped_refptr<Task> task);
  void OnTaskComplete(Callback callback, scoped_refptr<Task> task, Error error);
  void NotifyObservers(const CrxUpdateItem& item);
  void RunOrEnqueueTask(scoped_refptr<Task> task);

  SEQUENCE_CHECKER(sequence_checker_);

  // True if `Stop()` has been called.
  bool is_stopped_ = false;

  scoped_refptr<Configurator> config_;

  // Contains the tasks that are pending. In the current implementation,
  // only update tasks (background tasks) are queued up. These tasks are
  // pending while they are in this queue. They have not been picked up yet
  // by the update engine.
  base::circular_deque<scoped_refptr<Task>> task_queue_;

  // Contains all tasks in progress. These are the tasks that the update engine
  // is executing at one moment. Install tasks are run concurrently, update
  // tasks are always serialized, and update tasks are queued up if install
  // tasks are running. In addition, concurrent install tasks for the same id
  // are not allowed.
  std::set<scoped_refptr<Task>> tasks_;
  scoped_refptr<PingManager> ping_manager_;
  scoped_refptr<UpdateEngine> update_engine_;
  base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_INTERNAL_H_
