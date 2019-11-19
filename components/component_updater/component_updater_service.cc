// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/component_updater_service.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/component_updater/component_updater_service_internal.h"
#include "components/update_client/configurator.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "components/update_client/utils.h"
#include "url/gurl.h"

using CrxInstaller = update_client::CrxInstaller;
using UpdateClient = update_client::UpdateClient;

namespace {

enum UpdateType {
  UPDATE_TYPE_MANUAL = 0,
  UPDATE_TYPE_AUTOMATIC,
  UPDATE_TYPE_COUNT,
};

}  // namespace

namespace component_updater {

ComponentInfo::ComponentInfo(const std::string& id,
                             const std::string& fingerprint,
                             const base::string16& name,
                             const base::Version& version)
    : id(id), fingerprint(fingerprint), name(name), version(version) {}
ComponentInfo::ComponentInfo(const ComponentInfo& other) = default;
ComponentInfo::ComponentInfo(ComponentInfo&& other) = default;
ComponentInfo::~ComponentInfo() {}

CrxUpdateService::CrxUpdateService(scoped_refptr<Configurator> config,
                                   std::unique_ptr<UpdateScheduler> scheduler,
                                   scoped_refptr<UpdateClient> update_client)
    : config_(config),
      scheduler_(std::move(scheduler)),
      update_client_(update_client) {
  AddObserver(this);
}

CrxUpdateService::~CrxUpdateService() {
  DCHECK(thread_checker_.CalledOnValidThread());

  for (auto& item : ready_callbacks_) {
    std::move(item.second).Run();
  }

  RemoveObserver(this);

  Stop();
}

void CrxUpdateService::AddObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  update_client_->AddObserver(observer);
}

void CrxUpdateService::RemoveObserver(Observer* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  update_client_->RemoveObserver(observer);
}

void CrxUpdateService::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "CrxUpdateService starting up. "
          << "First update attempt will take place in "
          << config_->InitialDelay() << " seconds. "
          << "Next update attempt will take place in "
          << config_->NextCheckDelay() << " seconds. ";

  scheduler_->Schedule(
      base::TimeDelta::FromSeconds(config_->InitialDelay()),
      base::TimeDelta::FromSeconds(config_->NextCheckDelay()),
      base::Bind(base::IgnoreResult(&CrxUpdateService::CheckForUpdates),
                 base::Unretained(this)),
      base::DoNothing());
}

// Stops the update loop. In flight operations will be completed.
void CrxUpdateService::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "CrxUpdateService stopping";
  scheduler_->Stop();
  update_client_->Stop();
}

// Adds a component to be checked for upgrades. If the component exists it
// it will be replaced.
bool CrxUpdateService::RegisterComponent(const CrxComponent& component) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (component.app_id.empty() || !component.version.IsValid() ||
      !component.installer) {
    return false;
  }

  // Update the registration data if the component has been registered before.
  auto it = components_.find(component.app_id);
  if (it != components_.end()) {
    it->second = component;
    return true;
  }

  components_.insert(std::make_pair(component.app_id, component));
  components_order_.push_back(component.app_id);
  for (const auto& mime_type : component.handled_mime_types)
    component_ids_by_mime_type_[mime_type] = component.app_id;

  // Create an initial state for this component. The state is mutated in
  // response to events from the UpdateClient instance.
  CrxUpdateItem item;
  item.id = component.app_id;
  item.component = component;
  const auto inserted =
      component_states_.insert(std::make_pair(component.app_id, item));
  DCHECK(inserted.second);

  // Start the timer if this is the first component registered. The first timer
  // event occurs after an interval defined by the component update
  // configurator. The subsequent timer events are repeated with a period
  // defined by the same configurator.
  if (components_.size() == 1)
    Start();

  return true;
}

bool CrxUpdateService::UnregisterComponent(const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = components_.find(id);
  if (it == components_.end())
    return false;

  DCHECK_EQ(id, it->first);

  // Delay the uninstall of the component if the component is being updated.
  if (update_client_->IsUpdating(id)) {
    components_pending_unregistration_.push_back(id);
    return true;
  }

  return DoUnregisterComponent(it->second);
}

bool CrxUpdateService::DoUnregisterComponent(const CrxComponent& component) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const auto id = GetCrxComponentID(component);
  DCHECK(ready_callbacks_.find(id) == ready_callbacks_.end());

  const bool result = component.installer->Uninstall();

  const auto pos =
      std::find(components_order_.begin(), components_order_.end(), id);
  if (pos != components_order_.end())
    components_order_.erase(pos);

  components_.erase(id);
  component_states_.erase(id);

  return result;
}

std::vector<std::string> CrxUpdateService::GetComponentIDs() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<std::string> ids;
  for (const auto& it : components_)
    ids.push_back(it.first);
  return ids;
}

std::unique_ptr<ComponentInfo> CrxUpdateService::GetComponentForMimeType(
    const std::string& mime_type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  const auto it = component_ids_by_mime_type_.find(mime_type);
  if (it == component_ids_by_mime_type_.end())
    return nullptr;
  const auto component = GetComponent(it->second);
  if (!component)
    return nullptr;
  return std::make_unique<ComponentInfo>(
      GetCrxComponentID(*component), component->fingerprint,
      base::UTF8ToUTF16(component->name), component->version);
}

std::vector<ComponentInfo> CrxUpdateService::GetComponents() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<ComponentInfo> result;
  for (const auto& it : components_) {
    result.push_back(ComponentInfo(it.first, it.second.fingerprint,
                                   base::UTF8ToUTF16(it.second.name),
                                   it.second.version));
  }
  return result;
}

OnDemandUpdater& CrxUpdateService::GetOnDemandUpdater() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return *this;
}

base::Optional<CrxComponent> CrxUpdateService::GetComponent(
    const std::string& id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  const auto it = components_.find(id);
  if (it != components_.end())
    return it->second;
  return base::nullopt;
}

const CrxUpdateItem* CrxUpdateService::GetComponentState(
    const std::string& id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  const auto it(component_states_.find(id));
  return it != component_states_.end() ? &it->second : nullptr;
}

void CrxUpdateService::MaybeThrottle(const std::string& id,
                                     base::OnceClosure callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  const auto it = components_.find(id);
  if (it != components_.end()) {
    DCHECK_EQ(it->first, id);
    if (OnDemandUpdateWithCooldown(id)) {
      ready_callbacks_.insert(std::make_pair(id, std::move(callback)));
      return;
    }
  }

  // Unblock the request if the request can't be throttled.
  std::move(callback).Run();
}

void CrxUpdateService::OnDemandUpdate(const std::string& id,
                                      Priority priority,
                                      Callback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!GetComponent(id)) {
    if (!callback.is_null()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    update_client::Error::INVALID_ARGUMENT));
    }
    return;
  }

  OnDemandUpdateInternal(id, priority, std::move(callback));
}

bool CrxUpdateService::OnDemandUpdateWithCooldown(const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  DCHECK(GetComponent(id));

  // Check if the request is too soon.
  const auto* component_state(GetComponentState(id));
  if (component_state && !component_state->last_check.is_null()) {
    base::TimeDelta delta =
        base::TimeTicks::Now() - component_state->last_check;
    if (delta < base::TimeDelta::FromSeconds(config_->OnDemandDelay()))
      return false;
  }

  OnDemandUpdateInternal(id, Priority::FOREGROUND, Callback());
  return true;
}

void CrxUpdateService::OnDemandUpdateInternal(const std::string& id,
                                              Priority priority,
                                              Callback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.Calls", UPDATE_TYPE_MANUAL,
                            UPDATE_TYPE_COUNT);

  auto crx_data_callback = base::BindOnce(&CrxUpdateService::GetCrxComponents,
                                          base::Unretained(this));
  auto update_complete_callback = base::BindOnce(
      &CrxUpdateService::OnUpdateComplete, base::Unretained(this),
      std::move(callback), base::TimeTicks::Now());

  if (priority == Priority::FOREGROUND)
    update_client_->Install(id, std::move(crx_data_callback),
                            std::move(update_complete_callback));
  else if (priority == Priority::BACKGROUND)
    update_client_->Update({id}, std::move(crx_data_callback), false,
                           std::move(update_complete_callback));
  else
    NOTREACHED();
}

bool CrxUpdateService::CheckForUpdates(
    UpdateScheduler::OnFinishedCallback on_finished) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(xiaochu): remove this log after https://crbug.com/851151 is fixed.
  VLOG(1) << "CheckForUpdates: automatic updatecheck for components.";

  UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.Calls", UPDATE_TYPE_AUTOMATIC,
                            UPDATE_TYPE_COUNT);

  std::vector<std::string> secure_ids;    // Requires HTTPS for update checks.
  std::vector<std::string> unsecure_ids;  // Can fallback to HTTP.
  for (const auto id : components_order_) {
    DCHECK(components_.find(id) != components_.end());

    const auto component = GetComponent(id);
    if (!component || component->requires_network_encryption)
      secure_ids.push_back(id);
    else
      unsecure_ids.push_back(id);
  }

  if (unsecure_ids.empty() && secure_ids.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(on_finished));
    return true;
  }

  Callback on_finished_callback = base::BindOnce(
      [](UpdateScheduler::OnFinishedCallback on_finished,
         update_client::Error error) { std::move(on_finished).Run(); },
      std::move(on_finished));

  if (!unsecure_ids.empty()) {
    update_client_->Update(
        unsecure_ids,
        base::BindOnce(&CrxUpdateService::GetCrxComponents,
                       base::Unretained(this)),
        false,
        base::BindOnce(
            &CrxUpdateService::OnUpdateComplete, base::Unretained(this),
            secure_ids.empty() ? std::move(on_finished_callback) : Callback(),
            base::TimeTicks::Now()));
  }

  if (!secure_ids.empty()) {
    update_client_->Update(
        secure_ids,
        base::BindOnce(&CrxUpdateService::GetCrxComponents,
                       base::Unretained(this)),
        false,
        base::BindOnce(&CrxUpdateService::OnUpdateComplete,
                       base::Unretained(this), std::move(on_finished_callback),
                       base::TimeTicks::Now()));
  }

  return true;
}

bool CrxUpdateService::GetComponentDetails(const std::string& id,
                                           CrxUpdateItem* item) const {
  DCHECK(thread_checker_.CalledOnValidThread());

  // First, if this component is currently being updated, return its state from
  // the update client.
  if (update_client_->GetCrxUpdateState(id, item))
    return true;

  // Otherwise, return the last seen state of the component, if such a
  // state exists.
  const auto component_states_it = component_states_.find(id);
  if (component_states_it != component_states_.end()) {
    *item = component_states_it->second;
    return true;
  }

  return false;
}

std::vector<base::Optional<CrxComponent>> CrxUpdateService::GetCrxComponents(
    const std::vector<std::string>& ids) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<base::Optional<CrxComponent>> components;
  for (const auto& id : ids)
    components.push_back(GetComponent(id));
  return components;
}

void CrxUpdateService::OnUpdateComplete(Callback callback,
                                        const base::TimeTicks& start_time,
                                        update_client::Error error) {
  DCHECK(thread_checker_.CalledOnValidThread());
  VLOG(1) << "Update completed with error " << static_cast<int>(error);

  UMA_HISTOGRAM_BOOLEAN("ComponentUpdater.UpdateCompleteResult",
                        error != update_client::Error::NONE);
  UMA_HISTOGRAM_ENUMERATION("ComponentUpdater.UpdateCompleteError", error,
                            update_client::Error::MAX_VALUE);
  UMA_HISTOGRAM_LONG_TIMES_100("ComponentUpdater.UpdateCompleteTime",
                               base::TimeTicks::Now() - start_time);

  for (const auto id : components_pending_unregistration_) {
    if (!update_client_->IsUpdating(id)) {
      const auto component = GetComponent(id);
      if (component)
        DoUnregisterComponent(*component);
    }
  }

  if (!callback.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), error));
  }
}

void CrxUpdateService::OnEvent(Events event, const std::string& id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Unblock all throttles for the component.
  if (event == Observer::Events::COMPONENT_UPDATED ||
      event == Observer::Events::COMPONENT_NOT_UPDATED ||
      event == Observer::Events::COMPONENT_UPDATE_ERROR) {
    auto callbacks = ready_callbacks_.equal_range(id);
    for (auto it = callbacks.first; it != callbacks.second; ++it) {
      std::move(it->second).Run();
    }
    ready_callbacks_.erase(id);
  }

  CrxUpdateItem update_item;
  if (!update_client_->GetCrxUpdateState(id, &update_item))
    return;

  // Update the state of the item.
  const auto it = component_states_.find(id);
  if (it != component_states_.end())
    it->second = update_item;

  // Update the component registration with the new version.
  if (event == Observer::Events::COMPONENT_UPDATED) {
    const auto it = components_.find(id);
    if (it != components_.end()) {
      it->second.version = update_item.next_version;
      it->second.fingerprint = update_item.next_fp;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

// The component update factory. Using the component updater as a singleton
// is the job of the browser process.
// TODO(sorin): consider making this a singleton.
std::unique_ptr<ComponentUpdateService> ComponentUpdateServiceFactory(
    scoped_refptr<Configurator> config,
    std::unique_ptr<UpdateScheduler> scheduler) {
  DCHECK(config);
  DCHECK(scheduler);
  auto update_client = update_client::UpdateClientFactory(config);
  return std::make_unique<CrxUpdateService>(config, std::move(scheduler),
                                            std::move(update_client));
}

}  // namespace component_updater
