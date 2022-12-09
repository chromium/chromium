// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_CAPABILITY_ACCESS_CACHE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_CAPABILITY_ACCESS_CACHE_H_

#include <map>
#include <set>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/capability_access.h"
#include "components/services/app_service/public/cpp/capability_access_update.h"

namespace apps {

// Caches all of the apps::mojom::CapabilityAccessPtr's seen by an
// apps::mojom::Subscriber. A Subscriber sees a stream of "deltas", or changes
// in access state. This cache also keeps the "sum" of those previous deltas, so
// that observers of this object are presented with CapabilityAccessUpdate's,
// i.e. "state-and-delta"s.
//
// It can also be queried synchronously, providing answers from its in-memory
// cache, even though the underlying CapabilityAccess (and its App Publishers)
// communicate asynchronously, possibly across process boundaries, via Mojo
// IPC. Synchronous APIs can be more suitable for e.g. UI programming that
// should not block an event loop on I/O.
//
// This class is not thread-safe.
//
// See components/services/app_service/README.md for more details.
//
// // TODO(crbug.com/1253250): Remove all mojom related code.
// 1. Modify comments.
// 2. Replace mojom related functions with non-mojom functions.
class COMPONENT_EXPORT(APP_UPDATE) AppCapabilityAccessCache {
 public:
  class COMPONENT_EXPORT(APP_UPDATE) Observer : public base::CheckedObserver {
   public:
    // The apps::CapabilityAccessUpdate argument shouldn't be accessed after
    // OnCapabilityAccessUpdate returns.
    virtual void OnCapabilityAccessUpdate(
        const CapabilityAccessUpdate& update) = 0;

    // Called when the AppCapabilityAccessCache object (the thing that this
    // observer observes) will be destroyed. In response, the observer, |this|,
    // should call "cache->RemoveObserver(this)", whether directly or indirectly
    // (e.g. via base::ScopedObservation::Remove or via Observe(nullptr)).
    virtual void OnAppCapabilityAccessCacheWillBeDestroyed(
        AppCapabilityAccessCache* cache) = 0;

   protected:
    // Use this constructor when the observer |this| is tied to a single
    // AppCapabilityAccessCache for its entire lifetime, or until the observee
    // (the AppCapabilityAccessCache) is destroyed, whichever comes first.
    explicit Observer(AppCapabilityAccessCache* cache);

    // Use this constructor when the observer |this| wants to observe a
    // AppCapabilityAccessCache for part of its lifetime. It can then call
    // Observe() to start and stop observing.
    Observer();

    ~Observer() override;

    // Start observing a different AppCapabilityAccessCache. |cache| may be
    // nullptr, meaning to stop observing.
    void Observe(AppCapabilityAccessCache* cache);

   private:
    raw_ptr<AppCapabilityAccessCache> cache_ = nullptr;
  };

  AppCapabilityAccessCache();
  ~AppCapabilityAccessCache();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetAccountId(const AccountId& account_id);

  // Returns app ids which are accessing the camera.
  std::set<std::string> GetAppsAccessingCamera();

  // Returns app ids which are accessing the microphone.
  std::set<std::string> GetAppsAccessingMicrophone();

  // Notifies all observers of state-and-delta CapabilityAccessUpdate's (the
  // state comes from the internal cache, the delta comes from the argument) and
  // then merges the cached states with the deltas.
  //
  // Notification and merging might be delayed until after OnCapabilityAccesses
  // returns. For example, suppose that the initial set of states is (a0, b0,
  // c0) for three app_id's ("a", "b", "c"). Now suppose OnCapabilityAccesses is
  // called with two updates (b1, c1), and when notified of b1, an observer
  // calls OnCapabilityAccesses again with (c2, d2). The c1 delta should be
  // processed before the c2 delta, as it was sent first: c2 should be merged
  // (with "newest wins" semantics) onto c1 and not vice versa. This means that
  // processing c2 (scheduled by the second OnCapabilityAccesses call) should
  // wait until the first OnCapabilityAccesses call has finished processing b1
  // (and then c1), which means that processing c2 is delayed until after the
  // second OnCapabilityAccesses call returns.
  //
  // The callee will consume the deltas. An apps::mojom::CapabilityAccessPtr has
  // the ownership semantics of a unique_ptr, and will be deleted when out of
  // scope. The caller presumably calls OnCapabilityAccesses(std::move(deltas)).
  void OnCapabilityAccesses(std::vector<CapabilityAccessPtr> deltas);

  // Calls f, a void-returning function whose arguments are (const
  // apps::CapabilityAccessUpdate&), on each app in AppCapabilityAccessCache.
  //
  // f's argument is an apps::CapabilityAccessUpdate instead of an
  // apps::mojom::CapabilityAccessPtr so that callers can more easily share code
  // with Observer::OnCapabilityAccessUpdate (which also takes an
  // apps::CapabilityAccessUpdate), and an apps::CapabilityAccessUpdate also has
  // a StateIsNull method.
  //
  // The apps::CapabilityAccessUpdate argument to f shouldn't be accessed after
  // f returns.
  //
  // f must be synchronous, and if it asynchronously calls ForEachApp again,
  // it's not guaranteed to see a consistent state.
  template <typename FunctionType>
  void ForEachApp(FunctionType f) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

    for (const auto& s_iter : states_) {
      const CapabilityAccess* state = s_iter.second.get();

      auto d_iter = deltas_in_progress_.find(s_iter.first);
      const CapabilityAccess* delta =
          (d_iter != deltas_in_progress_.end()) ? d_iter->second : nullptr;

      f(CapabilityAccessUpdate(state, delta, account_id_));
    }

    for (const auto& d_iter : deltas_in_progress_) {
      const CapabilityAccess* delta = d_iter.second;

      auto s_iter = states_.find(d_iter.first);
      if (s_iter != states_.end()) {
        continue;
      }

      f(CapabilityAccessUpdate(nullptr, delta, account_id_));
    }
  }

  // Calls f, a void-returning function whose arguments are (const
  // apps::CapabilityAccessUpdate&), on the app in AppCapabilityAccessCache with
  // the given app_id. It will return true (and call f) if there is such an app,
  // otherwise it will return false (and not call f). The CapabilityAccessUpdate
  // argument to f has the same semantics as for ForEachApp, above.
  //
  // f must be synchronous, and if it asynchronously calls ForEachApp again,
  // it's not guaranteed to see a consistent state.
  template <typename FunctionType>
  bool ForOneApp(const std::string& app_id, FunctionType f) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

    auto s_iter = states_.find(app_id);
    const CapabilityAccess* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;

    auto d_iter = deltas_in_progress_.find(app_id);
    const CapabilityAccess* delta =
        (d_iter != deltas_in_progress_.end()) ? d_iter->second : nullptr;

    if (state || delta) {
      f(CapabilityAccessUpdate(state, delta, account_id_));
      return true;
    }
    return false;
  }

 private:
  void DoOnCapabilityAccesses(std::vector<CapabilityAccessPtr> deltas);

  base::ObserverList<Observer> observers_;

  // Maps from app_id to the latest state: the "sum" of all previous deltas.
  std::map<std::string, CapabilityAccessPtr> states_;

  // Track the deltas being processed or are about to be processed by
  // OnCapabilityAccesses. They are separate to manage the "notification and
  // merging might be delayed until after OnCapabilityAccesses returns" concern
  // described above.
  //
  // OnCapabilityAccesses calls DoOnCapabilityAccesses zero or more times. If
  // we're nested, so that there's multiple OnCapabilityAccesses call to this
  // AppCapabilityAccessCache in the call stack, the deeper OnCapabilityAccesses
  // call simply adds work to deltas_pending_ and returns without calling
  // DoOnCapabilityAccesses. If we're not nested, OnCapabilityAccesses calls
  // DoOnCapabilityAccesses one or more times; "more times" happens if
  // DoOnAccesses notifying observers leads to more OnCapabilityAccesses calls
  // that enqueue deltas_pending_ work. The deltas_in_progress_ map (keyed by
  // app_id) contains those deltas being considered by DoOnCapabilityAccesses.
  //
  // Nested OnCapabilityAccesses calls are expected to be rare (but still dealt
  // with sensibly). In the typical case, OnCapabilityAccesses should call
  // DoOnCapabilityAccesses exactly once, and deltas_pending_ will stay empty.
  std::map<std::string, CapabilityAccess*> deltas_in_progress_;
  std::vector<CapabilityAccessPtr> deltas_pending_;

  AccountId account_id_;

  SEQUENCE_CHECKER(my_sequence_checker_);
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_CAPABILITY_ACCESS_CACHE_H_
