// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PARALLEL_FETCH_MANAGER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PARALLEL_FETCH_MANAGER_H_

#include <memory>

#include "base/containers/id_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"

namespace supervised_user {

// Component for managing multiple fetches at once.
//
// After each fetch, the reference kept in internal map is cleared. This will
// also happen when this manager is destroyed. In the latter case, callbacks
// won't be executed (the pending requests will be canceled).
template <typename Request, typename Response>
class ParallelFetchManager {
 private:
  using Fetcher = ProtoFetcher<Response>;
  using KeyType = base::IDMap<std::unique_ptr<Fetcher>>::KeyType;

 public:
  // Provides fresh instances of a started fetcher for each fetch.
  using FetcherFactory = base::RepeatingCallback<std::unique_ptr<Fetcher>(
      const Request&,
      typename Fetcher::Callback callback)>;

  ParallelFetchManager() = delete;
  explicit ParallelFetchManager(FetcherFactory fetcher_factory)
      : fetcher_factory_(fetcher_factory) {}

  ParallelFetchManager(const ParallelFetchManager&) = delete;
  ParallelFetchManager& operator=(const ParallelFetchManager&) = delete;
  ~ParallelFetchManager() = default;

  // Starts the fetch. Underlying fetcher is stored internally, and will be
  // cleaned up after finish or when this manager is destroyed.
  void Fetch(const Request& request, Fetcher::Callback callback) {
    CHECK(callback) << "Use base::DoNothing() instead of empty callback.";

    KeyType key = ++id_;
    typename Fetcher::Callback callback_with_cleanup =
        std::move(callback).Then(base::BindOnce(
            &ParallelFetchManager::Remove, weak_factory_.GetWeakPtr(), key));
    requests_in_flight_.AddWithID(
        fetcher_factory_.Run(request, std::move(callback_with_cleanup)), key);
  }

 private:
  // Remove fetcher under key from requests_in_flight_.
  void Remove(KeyType key) { requests_in_flight_.Remove(key); }

  base::IDMap<std::unique_ptr<Fetcher>, KeyType> requests_in_flight_;
  KeyType id_{0};
  FetcherFactory fetcher_factory_;
  base::WeakPtrFactory<ParallelFetchManager<Request, Response>> weak_factory_{
      this};
};

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_PARALLEL_FETCH_MANAGER_H_
