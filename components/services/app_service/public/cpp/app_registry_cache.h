// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_REGISTRY_CACHE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_REGISTRY_CACHE_H_

#include <map>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"

namespace apps {

// An in-memory cache of all the metadata about installed apps known to App
// Service. Can be queried synchronously for information about the current
// state, and can be observed to receive updates about changes to that app
// state.
//
// AppServiceProxy sees a stream of `apps::AppPtr` "deltas", or changes in app
// state, received from publishers. This cache stores the "sum" of those
// previous deltas. When a new delta is received, observers are presented with
// an `apps:::AppUpdate` containing information about what has changed, and
// then the new delta is "added" to the stored state.
//
// This class is not thread-safe.
//
// See components/services/app_service/README.md for more details.
class COMPONENT_EXPORT(APP_UPDATE) AppRegistryCache {
 public:
  // Observer for changes to app state in the AppRegistryCache.
  class COMPONENT_EXPORT(APP_UPDATE) Observer : public base::CheckedObserver {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    // Called whenever AppRegistryCache receives an update for any app. `update`
    // exposes the latest field values and whether they have changed in this
    // update (as per the docs on `apps::AppUpdate`). The `update` argument
    // shouldn't be accessed after OnAppUpdate returns.
    virtual void OnAppUpdate(const AppUpdate& update) {}

    // Called when the AppRegistryCache first receives a set of apps for
    // `app_type`. This is usually when a publisher first publishes its apps but
    // may also happen if the AppRegistryCache gets instantiated after this
    // event (e.g. after a Lacros restart).
    // Note that this will not be called for app types initialized prior to this
    // observer being registered. Observers should call
    // AppRegistryCache::InitializedAppTypes() at the time of starting
    // observation to get a set of the app types which have been initialized.
    virtual void OnAppTypeInitialized(apps::AppType app_type) {}

    // Called when the AppRegistryCache object (the thing that this observer
    // observes) will be destroyed. In response, the observer, `this`, should
    // call "cache->RemoveObserver(this)", whether directly or indirectly (e.g.
    // via base::ScopedObservation::Reset).
    virtual void OnAppRegistryCacheWillBeDestroyed(AppRegistryCache* cache) = 0;

   protected:
    Observer();

    ~Observer() override;
  };

  AppRegistryCache();

  AppRegistryCache(const AppRegistryCache&) = delete;
  AppRegistryCache& operator=(const AppRegistryCache&) = delete;

  ~AppRegistryCache();

  // Prefer using a base::ScopedObservation for idiomatic observer behavior.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies all observers of state-and-delta AppUpdate's (the state comes
  // from the internal cache, the delta comes from the argument) and then
  // merges the cached states with the deltas.
  //
  // Notification and merging might be delayed until after OnApps returns. For
  // example, suppose that the initial set of states is (a0, b0, c0) for three
  // app_id's ("a", "b", "c"). Now suppose OnApps is called with two updates
  // (b1, c1), and when notified of b1, an observer calls OnApps again with
  // (c2, d2). The c1 delta should be processed before the c2 delta, as it was
  // sent first: c2 should be merged (with "newest wins" semantics) onto c1 and
  // not vice versa. This means that processing c2 (scheduled by the second
  // OnApps call) should wait until the first OnApps call has finished
  // processing b1 (and then c1), which means that processing c2 is delayed
  // until after the second OnApps call returns.
  //
  // The callee will consume the deltas. An apps::AppPtr has the ownership
  // semantics of a unique_ptr, and will be deleted when out of scope. The
  // caller presumably calls OnApps(std::move(deltas)).
  void OnApps(std::vector<AppPtr> deltas,
              apps::AppType app_type,
              bool should_notify_initialized);

  AppType GetAppType(const std::string& app_id);

  std::vector<AppPtr> GetAllApps();

  void SetAccountId(const AccountId& account_id);

  // Calls f, a void-returning function whose arguments are (const
  // apps::AppUpdate&), on each app in the cache.
  //
  // f's argument is an apps::AppUpdate instead of an apps::AppPtr so
  // that callers can more easily share code with Observer::OnAppUpdate (which
  // also takes an apps::AppUpdate), and an apps::AppUpdate also has a
  // StateIsNull method.
  //
  // The apps::AppUpdate argument to f shouldn't be accessed after f returns.
  //
  // f must be synchronous, and if it asynchronously calls ForEachApp again,
  // it's not guaranteed to see a consistent state.
  template <typename FunctionType>
  void ForEachApp(FunctionType f) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

    for (const auto& s_iter : states_) {
      const App* state = s_iter.second.get();

      auto d_iter = deltas_in_progress_.find(s_iter.first);
      const App* delta =
          (d_iter != deltas_in_progress_.end()) ? d_iter->second : nullptr;

      f(AppUpdate(state, delta, account_id_));
    }

    for (const auto& d_iter : deltas_in_progress_) {
      const App* delta = d_iter.second;

      if (base::Contains(states_, d_iter.first)) {
        continue;
      }

      f(AppUpdate(nullptr, delta, account_id_));
    }
  }

  // Calls f, a void-returning function whose arguments are (const
  // apps::AppUpdate&), on the app in the cache with the given app_id. It will
  // return true (and call f) if there is such an app, otherwise it will return
  // false (and not call f). The AppUpdate argument to f has the same semantics
  // as for ForEachApp, above.
  //
  // f must be synchronous, and if it asynchronously calls ForOneApp again,
  // it's not guaranteed to see a consistent state.
  template <typename FunctionType>
  bool ForOneApp(const std::string& app_id, FunctionType f) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

    auto s_iter = states_.find(app_id);
    const App* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;

    auto d_iter = deltas_in_progress_.find(app_id);
    const App* delta =
        (d_iter != deltas_in_progress_.end()) ? d_iter->second : nullptr;

    if (state || delta) {
      f(AppUpdate(state, delta, account_id_));
      return true;
    }
    return false;
  }

  // Returns the set of app types that have so far been initialized.
  const std::set<AppType>& InitializedAppTypes() const;

  bool IsAppTypeInitialized(AppType app_type) const;

  // Returns true if the cache contains an app with id `app_id` whose
  // `Readiness()` corresponds to an installed state.
  bool IsAppInstalled(const std::string& app_id) const;

  // Clears all apps from the cache.
  void ReinitializeForTesting();

 private:
  friend class AppRegistryCacheTest;
  friend class PublisherTest;

  void DoOnApps(std::vector<AppPtr> deltas);

  void OnAppTypeInitialized();

  base::ObserverList<Observer> observers_;

  // Maps from app_id to the latest state: the "sum" of all previous deltas.
  std::map<std::string, AppPtr> states_;

  // Track the deltas being processed or are about to be processed by OnApps.
  // They are separate to manage the "notification and merging might be delayed
  // until after OnApps returns" concern described above.
  //
  // OnApps calls DoOnApps zero or more times. If we're nested, so that there's
  // multiple OnApps call to this AppRegistryCache in the call stack, the
  // deeper OnApps call simply adds work to deltas_pending_ and returns
  // without calling DoOnApps. If we're not nested, OnApps calls DoOnApps one or
  // more times; "more times" happens if DoOnApps notifying observers leads to
  // more OnApps calls that enqueue deltas_pending_ work. The
  // deltas_in_progress_ map (keyed by app_id) contains those deltas being
  // considered by DoOnApps.
  //
  // Nested OnApps calls are expected to be rare (but still dealt with
  // sensibly). In the typical case, OnApps should call DoOnApps exactly once,
  // and deltas_pending_ will stay empty.
  std::map<std::string, App*> deltas_in_progress_;
  std::vector<AppPtr> deltas_pending_;

  // Saves app types which will finish initialization, and OnAppTypeInitialized
  // will be called to notify observers.
  std::set<AppType> in_progress_initialized_app_types_;

  // Saves app types which have finished initialization, and
  // OnAppTypeInitialized has be called to notify observers.
  std::set<AppType> initialized_app_types_;

  AccountId account_id_;

  SEQUENCE_CHECKER(my_sequence_checker_);
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_REGISTRY_CACHE_H_
