// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_REGISTRY_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_REGISTRY_H_

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "ash/public/cpp/shelf_types.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "ui/aura/window.h"

class InstanceRegistryTest;

namespace apps {

// The parameters to create or update the instance for the `app_id` and
// `window`, when calling InstanceRegistry::CreateOrUpdateInstance.
struct InstanceParams {
  InstanceParams(const std::string& app_id, aura::Window* window);

  InstanceParams(const InstanceParams&) = delete;
  InstanceParams& operator=(const InstanceParams&) = delete;

  ~InstanceParams();

  const std::string app_id;
  raw_ptr<aura::Window> window;
  std::optional<std::string> launch_id;
  std::optional<std::pair<InstanceState, base::Time>> state;
  std::optional<content::BrowserContext*> browser_context;
};

// An in-memory store of all the Instances (i.e. running apps) seen by
// AppServiceProxy. Can be queried synchronously for information about the
// currently running instances, and can be observed to receive updates about
// changes to Instance state.
//
// InstanceRegistry receives a stream of `app::Instance` delta updates from App
// Service, and stores the "sum" of these updates. When a new `apps::Instance`
// is received, observers are notified about the update, and then the delta is
// "added" to the stored state.
//
// This class is not thread-safe.
class InstanceRegistry {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called whenever the InstanceRegistry receives an update for any
    // instance. `update` exposes the latest field values and whether they have
    // changed in this update (as per the docs on `apps::InstanceUpdate`). The
    // `update` argument shouldn't be accessed after OnAppUpdate returns.
    virtual void OnInstanceUpdate(const InstanceUpdate& update) = 0;

    // Called when the InstanceRegistry object (the thing that this observer
    // observes) will be destroyed. In response, the observer, |this|, should
    // call "instance_registry->RemoveObserver(this)", whether directly or
    // indirectly (e.g. via base::ScopedObservation::Reset).
    virtual void OnInstanceRegistryWillBeDestroyed(InstanceRegistry* cache) = 0;

   protected:
    ~Observer() override;
  };

  InstanceRegistry();
  ~InstanceRegistry();

  InstanceRegistry(const InstanceRegistry&) = delete;
  InstanceRegistry& operator=(const InstanceRegistry&) = delete;

  // Prefer using a base::ScopedObservation to safely manage the observation,
  // instead of calling these methods directly.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  using InstancePtr = std::unique_ptr<Instance>;
  using InstanceIds = std::set<base::UnguessableToken>;

  // Creates a new instance for the `app_id` and `window` with a new instance id
  // if there is no exist instance. Otherwise, reuse the existing instance id
  // with `param` to update the instance.
  //
  // This function calls OnInstance to add the new instance or update the
  // existing instance.
  //
  // Note: For Lacros windows having multiple tabs/nstances, this interface
  // should not be called, since `window` might have multiple instances.
  void CreateOrUpdateInstance(InstanceParams&& param);

  // Notification and merging might be delayed until after OnInstance returns.
  // For example, suppose that the initial set of states is (a0, b0, c0) for
  // three app_id's ("a", "b", "c"). Now suppose OnInstance is called with an
  // update (b1), and when notified of b1, an observer calls OnInstance
  // again with (b2). The b1 delta should be processed before the b2 delta,
  // as it was sent first, and both b1 and b2 will be updated to the observer
  // following the sequence. This means that processing b2 (scheduled by the
  // second OnInstance call) should wait until the first OnInstance call has
  // finished processing b1, and then b2, which means that processing b2 is
  // delayed until after the second OnInstance call returns.
  //
  // The caller presumably calls OnInstance(std::move(delta)).
  void OnInstance(InstancePtr delta);

  // Returns instances for the |app_id|.
  std::set<raw_ptr<const Instance, SetExperimental>> GetInstances(
      const std::string& app_id);

  // Returns one state for the `window`.
  //
  // Note: This interface is used for the standalone window, or the ash Chrome
  // browser tab window, which has one instance only. For Lacros windows which
  // might have multiple instances for tabs, this interface should not be
  // called, since `window` might have multiple instances, and the InstanceState
  // returned in these cases will be arbitrary.
  InstanceState GetState(const aura::Window* window) const;

  // Returns the shelf id for the `window`.
  //
  // Note: This interface is used for the standalone window, or the ash Chrome
  // browser tab window, which has one instance only. For Lacros windows which
  // might have multiple instances for tabs, this interface should not be
  // called, since `window` might have multiple instances, and the ShelfID
  // returned in these cases will be arbitrary.
  ash::ShelfID GetShelfId(const aura::Window* window) const;

  // Return true if there is an instance for the `window`.
  bool Exists(const aura::Window* window) const;

  // Return true if there is any instance in the InstanceRegistry for |app_id|.
  bool ContainsAppId(const std::string& app_id) const;

  // Calls f, a void-returning function whose arguments are (const
  // apps::InstanceUpdate&), on each window in the instance_registry.
  //
  // f's argument is an apps::InstanceUpdate instead of an Instance* so that
  // callers can more easily share code with Observer::OnInstanceUpdate (which
  // also takes an apps::InstanceUpdate), and an apps::InstanceUpdate also has a
  // StateIsNull method.
  //
  // The apps::InstanceUpdate argument to f shouldn't be accessed after f
  // returns.
  //
  // f must be synchronous, and if it asynchronously calls ForEachInstance
  // again, it's not guaranteed to see a consistent state.
  template <typename FunctionType>
  void ForEachInstance(FunctionType f) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

    for (const auto& s_iter : states_) {
      const apps::Instance* state = s_iter.second.get();
      f(apps::InstanceUpdate(state, nullptr));
    }
  }

  // Calls f, a void-returning function whose arguments are (const
  // apps::InstanceUpdate&), on the instance in the instance_registry with the
  // given instance id. It will return true (and call f) if there is such an
  // instance, otherwise it will return false (and not call f). The
  // InstanceUpdate argument to f has the same semantics as for ForEachInstance,
  // above.
  //
  // f must be synchronous, and if it asynchronously calls ForOneInstance again,
  // it's not guaranteed to see a consistent state.
  template <typename FunctionType>
  bool ForOneInstance(const base::UnguessableToken& instance_id,
                      FunctionType f) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

    auto s_iter = states_.find(instance_id);
    apps::Instance* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
    if (state) {
      f(apps::InstanceUpdate(state, nullptr));
      return true;
    }
    return false;
  }

  // Calls f, a void-returning function whose arguments are (const
  // apps::InstanceUpdate&), on instances in the instance_registry with the
  // given window. It will return true (and call f) if there is such an
  // instance, otherwise it will return false (and not call f). The
  // InstanceUpdate argument to f has the same semantics as for ForEachInstance,
  // above.
  //
  // f must be synchronous, and if it asynchronously calls ForOneInstance again,
  // it's not guaranteed to see a consistent state.
  template <typename FunctionType>
  bool ForInstancesWithWindow(const aura::Window* window,
                              FunctionType f) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

    InstanceIds instance_ids;
    auto it = window_to_instance_ids_.find(window);
    if (it != window_to_instance_ids_.end()) {
      instance_ids = it->second;
    }

    // There could be some instances in `deltas_pending_`.
    for (const auto& delta : deltas_pending_) {
      if (delta->Window() == window) {
        instance_ids.insert(delta->InstanceId());
      }
    }

    // `old_state_` might be an instance to be deleted. But in OnInstanceUpdate,
    // the caller might still need that instance to know which instance will be
    // deleted.
    if (old_state_ && old_state_.get()) {
      instance_ids.insert(old_state_->InstanceId());
    }

    if (instance_ids.empty()) {
      return false;
    }

    for (const auto& instance_id : instance_ids) {
      auto s_iter = states_.find(instance_id);
      apps::Instance* state =
          (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
      if (state) {
        f(apps::InstanceUpdate(state, nullptr));
      }
    }
    return true;
  }

 private:
  friend class InstanceRegistryTest;

  void DoOnInstance(InstancePtr deltas);

  void MaybeRemoveInstance(const Instance* delta);

  void MaybeRemoveInstanceId(const base::UnguessableToken& instance_id,
                             aura::Window* window);

  base::ObserverList<Observer> observers_;

  // OnInstance calls DoOnInstance zero or more times. If we're nested,
  // in_progress is true, so that there's multiple OnInstance call to this
  // InstanceRegistry in the call stack, the deeper OnInstances call simply adds
  // work to deltas_pending_ and returns without calling DoOnInstance. If we're
  // not nested, in_progress is false, OnInstance calls DoOnInstance one or
  // more times; "more times" happens if DoOnInstance notifying observers leads
  // to more OnInstance calls that enqueue deltas_pending_ work.
  //
  // Nested OnInstance calls are expected to be rare (but still dealt with
  // sensibly). In the typical case, OnInstance should call DoOnInstance
  // exactly once, and deltas_pending_ will stay empty.
  bool in_progress_ = false;

  // Maps from the instance id to the latest state: the "sum" of all previous
  // deltas.
  std::map<const base::UnguessableToken, InstancePtr> states_;

  std::list<InstancePtr> deltas_pending_;

  // Maps from window to a set of instance id.
  std::map<const aura::Window*, InstanceIds> window_to_instance_ids_;

  // Maps from instance id to window, to check whether the window is changed for
  // the instance id. When a tab is pulled to a new Lacros window, the window
  // might be changed, and the instance id should be removed from
  // `window_to_instance_ids_`. `states_` can't be used to check window, because
  // some instances might be in `deltas_pending_`.
  std::map<const base::UnguessableToken, raw_ptr<aura::Window, CtnExperimental>>
      instance_id_to_window_;

  // Maps from app id to instances.
  std::map<const std::string,
           std::set<raw_ptr<const Instance, SetExperimental>>>
      app_id_to_instances_;

  std::unique_ptr<Instance> old_state_;

  SEQUENCE_CHECKER(my_sequence_checker_);
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_REGISTRY_H_
