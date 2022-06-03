// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_REGISTRY_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_REGISTRY_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_update.h"

namespace apps {

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
  using Instances = std::vector<InstancePtr>;

  // Notification and merging might be delayed until after OnInstances returns.
  // For example, suppose that the initial set of states is (a0, b0, c0) for
  // three app_id's ("a", "b", "c"). Now suppose OnInstances is called with two
  // updates (b1, c1), and when notified of b1, an observer calls OnInstances
  // again with (c2, d2). The c1 delta should be processed before the c2 delta,
  // as it was sent first, and both c1 and c2 will be updated to the observer
  // following the sequence. This means that processing c2 (scheduled by the
  // second OnInstances call) should wait until the first OnInstances call has
  // finished processing b1, and then c1, which means that processing c2 is
  // delayed until after the second OnInstances call returns.
  //
  // The caller presumably calls OnInstances(std::move(deltas)).
  void OnInstances(const Instances& deltas);

  // Return enclosing app windows for the |app_id|. If the app is in a browser
  // tab, the window returned will be the window of the browser.
  std::set<aura::Window*> GetEnclosingAppWindows(const std::string& app_id);

  // Return instance keys for the |app_id|.
  std::set<const Instance::InstanceKey> GetInstanceKeys(
      const std::string& app_id);

  // Return the state for the |instance_key|.
  InstanceState GetState(const Instance::InstanceKey& instance_key) const;

  // Return the shelf id for the |instance_key|.
  ash::ShelfID GetShelfId(const Instance::InstanceKey& instance_key) const;

  // Return true if there is an instance for the |instance_key|.
  bool Exists(const Instance::InstanceKey& instance_key) const;

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

    for (const auto& s_iter : states_) {
      apps::Instance* state = s_iter.second.get();
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

    auto s_iter = states_.find(instance_key);
    apps::Instance* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
    if (state) {
      f(apps::InstanceUpdate(state, nullptr));
      return true;
    }
    return false;
  }

 private:
  void DoOnInstances(const Instances& deltas);

  base::ObserverList<Observer> observers_;

  // OnInstances calls DoOnInstances zero or more times. If we're nested,
  // in_progress is true, so that there's multiple OnInstances call to this
  // InstanceRegistry in the call stack, the deeper OnInstances call simply adds
  // work to deltas_pending_ and returns without calling DoOnInstances. If we're
  // not nested, in_progress is false, OnInstances calls DoOnInstances one or
  // more times; "more times" happens if DoOnInstances notifying observers leads
  // to more OnInstances calls that enqueue deltas_pending_ work.
  //
  // Nested OnInstances calls are expected to be rare (but still dealt with
  // sensibly). In the typical case, OnInstances should call DoOnInstances
  // exactly once, and deltas_pending_ will stay empty.
  bool in_progress_ = false;

  // Maps from instance key to the latest state: the "sum" of all previous
  // deltas.
  std::map<const Instance::InstanceKey, InstancePtr> states_;
  Instances deltas_pending_;

  // Maps from app id to app instance key.
  std::map<const std::string, std::set<const Instance::InstanceKey>>
      app_id_to_app_instance_key_;

  SEQUENCE_CHECKER(my_sequence_checker_);
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_INSTANCE_REGISTRY_H_
