// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_INTERNAL_H_
#define COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_INTERNAL_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/component_updater/update_scheduler.h"
#include "components/update_client/persisted_data.h"

namespace base {
class TimeTicks;
}

namespace update_client {
enum class Error;
}

namespace component_updater {

class OnDemandUpdater;

class CrxUpdateService : public ComponentUpdateService,
                         public ComponentUpdateService::Observer,
                         public OnDemandUpdater {
  using Observer = ComponentUpdateService::Observer;

 public:
  CrxUpdateService(scoped_refptr<Configurator> config,
                   std::unique_ptr<UpdateScheduler> scheduler,
                   scoped_refptr<update_client::UpdateClient> update_client,
                   const std::string& brand);

  CrxUpdateService(const CrxUpdateService&) = delete;
  CrxUpdateService& operator=(const CrxUpdateService&) = delete;

  ~CrxUpdateService() override;

  // Overrides for ComponentUpdateService.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool RegisterComponent(const ComponentRegistration& component) override;
  bool UnregisterComponent(const std::string& id) override;
  std::vector<std::string> GetComponentIDs() const override;
  std::vector<ComponentInfo> GetComponents() const override;
  OnDemandUpdater& GetOnDemandUpdater() override;
  void MaybeThrottle(const std::string& id,
                     base::OnceClosure callback) override;
  bool GetComponentDetails(const std::string& id,
                           CrxUpdateItem* item) const override;
  base::Version GetRegisteredVersion(const std::string& app_id) override;
  base::Version GetMaxPreviousProductVersion(
      const std::string& app_id) override;

  // Overrides for Observer.
  void OnEvent(const CrxUpdateItem& item) override;

  // Overrides for OnDemandUpdater.
  void OnDemandUpdate(const std::string& id,
                      Priority priority,
                      Callback callback) override;

 private:
  void Start();
  void Stop();

  bool CheckForUpdates(UpdateScheduler::OnFinishedCallback on_finished);

  void OnDemandUpdateInternal(const std::string& id,
                              Priority priority,
                              Callback callback);
  bool OnDemandUpdateWithCooldown(const std::string& id);

  bool DoUnregisterComponent(const std::string& id);

  CrxComponent ToCrxComponent(const ComponentRegistration& component) const;

  std::optional<ComponentRegistration> GetComponent(
      const std::string& id) const;

  const CrxUpdateItem* GetComponentState(const std::string& id) const;

  void GetCrxComponents(
      const std::vector<std::string>& ids,
      base::OnceCallback<void(const std::vector<std::optional<CrxComponent>>&)>
          callback);
  void OnUpdateComplete(Callback callback,
                        const base::TimeTicks& start_time,
                        update_client::Error error);

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<Configurator> config_;
  std::unique_ptr<UpdateScheduler> scheduler_;
  scoped_refptr<update_client::UpdateClient> update_client_;

  std::string brand_;

  // A collection of every registered component.
  using Components = base::flat_map<std::string, ComponentRegistration>;
  Components components_;

  // Maintains the order in which components have been registered. The position
  // of a component id in this sequence indicates the priority of the component.
  // The sooner the component gets registered, the higher its priority, and
  // the closer this component is to the beginning of the vector.
  std::vector<std::string> components_order_;

  // Contains the components pending unregistration. If a component is not
  // busy installing or updating, it can be unregistered right away. Otherwise,
  // the component will be lazily unregistered after the its operations have
  // completed.
  std::vector<std::string> components_pending_unregistration_;

  // Contains the active resource throttles associated with a given component.
  using ResourceThrottleCallbacks =
      std::multimap<std::string, base::OnceClosure>;
  ResourceThrottleCallbacks ready_callbacks_;

  // Contains the state of the component.
  using ComponentStates = std::map<std::string, CrxUpdateItem>;
  ComponentStates component_states_;

  // Contains a map of media types to the component that implements a handler
  // for that media type. Only the most recently-registered component is
  // tracked. May include the IDs of un-registered components.
  std::map<std::string, std::string> component_ids_by_mime_type_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_COMPONENT_UPDATER_SERVICE_INTERNAL_H_
