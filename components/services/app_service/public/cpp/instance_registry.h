// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_REGISTRY_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_REGISTRY_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "ash/public/cpp/shelf_types.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  aura::Window* window;
  absl::optional<std::string> launch_id;
  absl::optional<std::pair<InstanceState, base::Time>> state;
  absl::optional<content::BrowserContext*> browser_context;
};

// InstanceRegistry keeps all of the Instances seen by AppServiceProxy.
// It also keeps the "sum" of those previous deltas, so that observers of this
// object can be updated with the InstanceUpdate structure. It can also be
// queried synchronously.
//
// This class is not thread-safe.
class InstanceRegistry {
 public:
  class Observer : public base::CheckedObserver {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // The InstanceUpdate argument shouldn't be accessed after OnInstanceUpdate
    // returns.
    virtual void OnInstanceUpdate(const InstanceUpdate& update) = 0;

    // Called when the InstanceRegistry object (the thing that this observer
    // observes) will be destroyed. In response, the observer, |this|, should
    // call "instance_registry->RemoveObserver(this)", whether directly or
    // indirectly (e.g. via base::ScopedObservation::Remove or via
    // Observe(nullptr)).
    virtual void OnInstanceRegistryWillBeDestroyed(InstanceRegistry* cache) = 0;

   protected:
    // Use this constructor when the observer |this| is tied to a single
    // InstanceRegistry for its entire lifetime, or until the observee (the
    // InstanceRegistry) is destroyed, whichever comes first.
    explicit Observer(InstanceRegistry* cache);

    // Use this constructor when the observer |this| wants to observe a
    // InstanceRegistry for part of its lifetime. It can then call Observe() to
    // start and stop observing.
    Observer();

    ~Observer() override;

    // Start observing a different InstanceRegistry. |instance_registry| may be
    // nullptr, meaning to stop observing.
    void Observe(InstanceRegistry* instance_registry);

   private:
    InstanceRegistry* instance_registry_ = nullptr;
  };

  InstanceRegistry();
  ~InstanceRegistry();

  InstanceRegistry(const InstanceRegistry&) = delete;
  InstanceRegistry& operator=(const InstanceRegistry&) = delete;

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

  // Return instance keys for the |app_id|.
  std::set<const Instance::InstanceKey> GetInstanceKeys(
      const std::string& app_id);

  // Return the state for the |instance_key|.
  InstanceState GetState(const Instance::InstanceKey& instance_key) const;

  // Return the shelf id for the |instance_key|.
  ash::ShelfID GetShelfId(const Instance::InstanceKey& instance_key) const;

  // Return true if there is an instance for the |instance_key|.
  // TODO(crbug.com/1251501): Will be removed soon.
  bool Exists(const Instance::InstanceKey& instance_key) const;

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
  void ForEachInstance(FunctionType f) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

    for (const auto& s_iter : instance_key_states_) {
      apps::Instance* state = s_iter.second;
      f(apps::InstanceUpdate(state, nullptr));
    }
  }

  // Calls f, a void-returning function whose arguments are (const
  // apps::InstanceUpdate&), on the instance in the instance_registry with the
  // given instance_key. It will return true (and call f) if there is such an
  // instance, otherwise it will return false (and not call f). The
  // InstanceUpdate argument to f has the same semantics as for ForEachInstance,
  // above.
  //
  // f must be synchronous, and if it asynchronously calls ForOneInstance again,
  // it's not guaranteed to see a consistent state.
  template <typename FunctionType>
  bool ForOneInstance(const Instance::InstanceKey& instance_key,
                      FunctionType f) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

    auto s_iter = instance_key_states_.find(instance_key);
    apps::Instance* state =
        (s_iter != instance_key_states_.end()) ? s_iter->second : nullptr;
    if (state) {
      f(apps::InstanceUpdate(state, nullptr));
      return true;
    }
    return false;
  }

 private:
  friend class InstanceRegistryTest;

  void DoOnInstance(InstancePtr deltas);

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

  // Maps from instance key to the latest state: the "sum" of all previous
  // deltas.
  // TODO(crbug.com/1251501): Will be removed soon.
  std::map<const Instance::InstanceKey, Instance*> instance_key_states_;

  // Maps from the instance id to the latest state: the "sum" of all previous
  // deltas.
  std::map<const base::UnguessableToken, InstancePtr> states_;

  std::list<InstancePtr> deltas_pending_;

  // Maps from app id to app instance key.
  // TODO(crbug.com/1251501): Will be removed soon.
  std::map<const std::string, std::set<const Instance::InstanceKey>>
      app_id_to_app_instance_key_;

  // Maps from window to a set of instance id.
  std::map<const aura::Window*, InstanceIds> window_to_instance_ids_;

  // Maps from instance id to window, to check whether the window is changed for
  // the instance id. When a tab is pulled to a new Lacros window, the window
  // might be changed, and the instance id should be removed from
  // `window_to_instance_ids_`. `states_` can't be used to check window, because
  // some instances might be in `deltas_pending_`.
  std::map<base::UnguessableToken, aura::Window*> instance_id_to_window_;

  SEQUENCE_CHECKER(my_sequence_checker_);
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_REGISTRY_H_
