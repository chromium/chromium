// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}  // namespace network

namespace password_manager {

class AffiliationBackend;

// A service that can be used to query the list of facets that are affiliated
// with a given facet, i.e., facets that belong to the same logical application.
// See affiliation_utils.h for details of what this means.
//
// The service must be accessed from the UI thread, and it can be utilized in
// two ways:
//
//   1.) On-demand fetching: For the one-off query that wishes to learn
//       affiliations of facet X when (potentially) issuing an on-demand
//       network request to the Affiliation API containing the URI of facet X
//       is acceptable from the privacy and/or performance perspective.
//
//       This mode of operation is achieved by invoking
//       GetAffiliationsAndBranding() with
//       StrategyOnCacheMiss::FETCH_OVER_NETWORK.
//
//   2.) Proactive fetching: For the compound query that is concerned with
//       checking, over time, whether or not each element in a sequence of
//       facets, W_1, W_2, ..., W_n, is affiliated with a fixed facet Y; and
//       when it is desired, for privacy and/or performance reasons, that only
//       facet Y be looked up against the Affiliation API and that subsequent
//       requests regarding each W_i not trigger additional requests.
//
//       This mode of operation can be useful when, for example, the password
//       manager has credentials stored for facet Y and wishes to check, for
//       each visited web site W_i, whether these credentials should be offered
//       to be autofilled onto W_i.
//
//       Example code:
//
//       class ExampleAffiliatedCredentialFiller
//           : public base::SupportsWeakPtr<...> {
//        public:
//         ExampleAffiliatedCredentialFiller(AffiliationService* service,
//                                           const FacetURI& y)
//             : service_(service), y_(y) {
//           cancel_handle_ = service_->Prefetch(y_, base::Time::Max());
//         }
//
//         ~ExampleAffiliatedCredentialFiller() { cancel_handle_.Run(); }
//
//         void ShouldFillInto(const FacetURI& wi, FillDelegate* delegate) {
//           service_->GetAffiliationsAndBranding(wi, StrategyOnCacheMiss::FAIL,
//               base::Bind(
//                   &ExampleAffiliatedCredentialFiller::OnAffiliationResult,
//                   AsWeakPtr(),
//                   delegate));
//         }
//
//         void OnAffiliationResult(FillDelegate* delegate,
//                                  const AffiliatedFacets& results,
//                                  bool success) {
//           if (success && std::count(results.begin(), results.end(), y_))
//             delegate->FillCredentialsFor(y_);
//         }
//
//        private:
//         AffiliationService* service_;
//         const FacetURI& y_;
//         CancelPrefetchingHandle cancel_handle_;
//       };
class AffiliationService : public KeyedService {
 public:
  using ResultCallback =
      base::OnceCallback<void(const AffiliatedFacets& /* results */,
                              bool /* success */)>;

  // Controls whether to send a network request or fail on a cache miss.
  enum class StrategyOnCacheMiss { FETCH_OVER_NETWORK, FAIL };

  // The |backend_task_runner| should be a task runner corresponding to a thread
  // that can take blocking I/O, and is normally Chrome's DB thread.
  explicit AffiliationService(
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);
  ~AffiliationService() override;

  // Initializes the service by creating its backend and transferring it to the
  // thread corresponding to |backend_task_runner_|.
  void Initialize(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      network::NetworkConnectionTracker* network_connection_tracker,
      const base::FilePath& db_path);

  // Looks up facets affiliated with the facet identified by |facet_uri| and
  // branding information, and invokes |result_callback| with the results. It is
  // guaranteed that the results will contain one facet with URI equal to
  // |facet_uri| when |result_callback| is invoked with success set to true.
  //
  // If the local cache contains fresh affiliation and branding information for
  // |facet_uri|, the request will be served from cache. Otherwise,
  // |cache_miss_policy| controls whether to issue an on-demand network request,
  // or to fail the request without fetching.
  virtual void GetAffiliationsAndBranding(
      const FacetURI& facet_uri,
      StrategyOnCacheMiss cache_miss_strategy,
      ResultCallback result_callback);

  // Prefetches affiliation information for the facet identified by |facet_uri|,
  // and keeps the information fresh by periodic re-fetches (as needed) until
  // the clock strikes |keep_fresh_until| (exclusive), until a matching call to
  // CancelPrefetch(), or until Chrome is shut down, whichever is sooner. It is
  // a supported use-case to pass base::Time::Max() as |keep_fresh_until|.
  //
  // Canceling can be useful when a password is deleted, so that resources are
  // no longer wasted on repeatedly refreshing affiliation information. Note
  // that canceling will not blow away data already stored in the cache unless
  // it becomes stale.
  virtual void Prefetch(const FacetURI& facet_uri,
                        const base::Time& keep_fresh_until);

  // Cancels the corresponding prefetch command, i.e., the one issued for the
  // same |facet_uri| and with the same |keep_fresh_until|.
  virtual void CancelPrefetch(const FacetURI& facet_uri,
                              const base::Time& keep_fresh_until);

  // Wipes results of on-demand fetches and expired prefetches from the cache,
  // but retains information corresponding to facets that are being kept fresh.
  // As no required data is deleted, there will be no network requests directly
  // triggered by this call. It will only potentially remove data
  // corresponding to the given |facet_uri|, but still only as long as the
  // data is no longer needed.
  virtual void TrimCacheForFacetURI(const FacetURI& facet_uri);

 private:
  // The backend, owned by this AffiliationService instance, but living on the
  // DB thread. It will be deleted asynchronously during shutdown on the DB
  // thread, so it will outlive |this| along with all its in-flight tasks.
  AffiliationBackend* backend_;

  // TaskRunner to be used to run the |backend_|.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AffiliationService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AffiliationService);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_SERVICE_H_
