// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_service_impl.h"

#include <algorithm>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/affiliations/core/browser/affiliation_backend.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace affiliations {

namespace {

void LogFetchResult(GetChangePasswordUrlMetric result) {
  base::UmaHistogramEnumeration(kGetChangePasswordURLMetricName, result);
}

// Creates a look-up (Facet URI : change password URL) map for facets from
// requested |groupings|. If a facet does not have change password URL it gets
// paired with another facet's URL, which belongs to the same group. In case
// none of the group's facets have change password URLs then those facets are
// not inserted to the map.
std::map<FacetURI, AffiliationServiceImpl::ChangePasswordUrlMatch>
CreateFacetUriToChangePasswordUrlMap(
    const std::vector<GroupedFacets>& groupings) {
  std::map<FacetURI, AffiliationServiceImpl::ChangePasswordUrlMatch> uri_to_url;
  for (const auto& grouped_facets : groupings) {
    std::vector<FacetURI> uris_without_urls;
    GURL fallback_url;
    for (const auto& facet : grouped_facets.facets) {
      if (!facet.change_password_url.is_valid()) {
        uris_without_urls.push_back(facet.uri);
        continue;
      }
      uri_to_url[facet.uri] = AffiliationServiceImpl::ChangePasswordUrlMatch{
          .change_password_url = facet.change_password_url,
          .group_url_override = false};
      fallback_url = facet.change_password_url;
    }
    if (fallback_url.is_valid()) {
      for (const auto& uri : uris_without_urls) {
        uri_to_url[uri] = AffiliationServiceImpl::ChangePasswordUrlMatch{
            .change_password_url = fallback_url, .group_url_override = true};
      }
    }
  }
  return uri_to_url;
}

FacetURI ConvertGURLToFacet(const GURL& url) {
  if (url.SchemeIs(url::kAndroidScheme)) {
    return FacetURI::FromPotentiallyInvalidSpec(url.possibly_invalid_spec());
  } else {
    // Path should be stripped before converting into FacetURI.
    return FacetURI::FromPotentiallyInvalidSpec(url.GetWithEmptyPath().spec());
  }
}

// Returns FacetURI corresponding to the top level domain of the `facet`. Empty
// if `facet` is android app, eTLD+1 can't be extracted, or `facet` is already
// top level domain.
FacetURI GetFacetForTopLevelDomain(FacetURI facet) {
  if (!facet.IsValidWebFacetURI()) {
    return FacetURI();
  }

  std::string top_domain =
      GetExtendedTopLevelDomain(GURL(facet.canonical_spec()), {});
  if (top_domain.empty()) {
    return FacetURI();
  }

  FacetURI result =
      FacetURI::FromPotentiallyInvalidSpec("https://" + top_domain);

  if (!result.is_valid() || result == facet) {
    return FacetURI();
  }

  return result;
}

void LogChangePasswordURLTypeUsed(
    const AffiliationServiceImpl::ChangePasswordUrlMatch& match) {
  if (match.group_url_override) {
    LogFetchResult(GetChangePasswordUrlMetric::kGroupUrlOverrideUsed);
  } else if (match.main_domain_override) {
    LogFetchResult(GetChangePasswordUrlMetric::kMainDomainUsed);
  } else {
    LogFetchResult(GetChangePasswordUrlMetric::kUrlOverrideUsed);
  }
}

}  // namespace

const char kGetChangePasswordURLMetricName[] =
    "PasswordManager.AffiliationService.GetChangePasswordUsage";

struct AffiliationServiceImpl::FetchInfo {
  FetchInfo(std::unique_ptr<AffiliationFetcherInterface> pending_fetcher,
            FacetURI facet,
            FacetURI main_domain_facet,
            base::OnceClosure result_callback)
      : fetcher(std::move(pending_fetcher)),
        requested_facet(std::move(facet)),
        top_level_domain(std::move(main_domain_facet)),
        callback(std::move(result_callback)) {
    std::vector<FacetURI> facets_to_request;
    facets_to_request.push_back(requested_facet);

    if (top_level_domain.is_valid()) {
      facets_to_request.push_back(top_level_domain);
    }

    fetcher->StartRequest(facets_to_request, kChangePasswordUrlRequestInfo);
  }

  FetchInfo(FetchInfo&& other) = default;

  FetchInfo& operator=(FetchInfo&& other) = default;

  ~FetchInfo() {
    // The check is essential here, because emplace_back calls move constructor
    // and destructor, respectively. Therefore, the check is necessary to
    // prevent accessing already moved object.
    if (callback)
      std::move(callback).Run();
  }

  ChangePasswordUrlMatch GetChangePasswordURL(
      std::unique_ptr<AffiliationFetcherDelegate::Result> result) {
    std::map<FacetURI, AffiliationServiceImpl::ChangePasswordUrlMatch>
        uri_to_url = CreateFacetUriToChangePasswordUrlMap(result->groupings);

    auto it = uri_to_url.find(requested_facet);
    if (it != uri_to_url.end()) {
      return it->second;
    }

    // Check if change password URL available for the main domain.
    it = uri_to_url.find(top_level_domain);
    if (it != uri_to_url.end()) {
      it->second.main_domain_override = true;
      return it->second;
    }

    return ChangePasswordUrlMatch();
  }

  std::unique_ptr<AffiliationFetcherInterface> fetcher;
  FacetURI requested_facet;
  FacetURI top_level_domain;
  // Callback is passed in PrefetchChangePasswordURLs and is run to indicate the
  // prefetch has finished or got canceled.
  base::OnceClosure callback;
};

// TODO(crbug.com/40789139): Create the backend task runner in Init and stop
// passing it in the constructor.
AffiliationServiceImpl::AffiliationServiceImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : url_loader_factory_(std::move(url_loader_factory)),
      fetcher_factory_(std::make_unique<AffiliationFetcherFactoryImpl>()),
      backend_task_runner_(std::move(backend_task_runner)) {}

AffiliationServiceImpl::~AffiliationServiceImpl() = default;

void AffiliationServiceImpl::Init(
    network::NetworkConnectionTracker* network_connection_tracker,
    const base::FilePath& db_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_ = std::make_unique<AffiliationBackend>(
      backend_task_runner_, base::DefaultClock::GetInstance(),
      base::DefaultTickClock::GetInstance());

  PostToBackend(&AffiliationBackend::Initialize, url_loader_factory_->Clone(),
                base::Unretained(network_connection_tracker), db_path);
}

void AffiliationServiceImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (backend_) {
    backend_task_runner_->DeleteSoon(FROM_HERE, std::move(backend_));
  }
}

void AffiliationServiceImpl::PrefetchChangePasswordURL(
    const GURL& url,
    base::OnceClosure callback) {
  FacetURI facet_uri = ConvertGURLToFacet(url);

  if (facet_uri.is_valid() &&
      !base::Contains(change_password_urls_, facet_uri)) {
    auto fetcher = fetcher_factory_->CreateInstance(url_loader_factory_, this);
    if (fetcher) {
      pending_fetches_.emplace_back(std::move(fetcher), std::move(facet_uri),
                                    GetFacetForTopLevelDomain(facet_uri),
                                    std::move(callback));
      return;
    }
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

GURL AffiliationServiceImpl::GetChangePasswordURL(const GURL& url) const {
  FacetURI uri = ConvertGURLToFacet(url);

  auto it = change_password_urls_.find(uri);
  if (it != change_password_urls_.end()) {
    LogChangePasswordURLTypeUsed(it->second);
    return it->second.change_password_url;
  }

  if (std::ranges::any_of(pending_fetches_, [&uri](const auto& info) {
        return uri == info.requested_facet;
      })) {
    LogFetchResult(GetChangePasswordUrlMetric::kNotFetchedYet);
  } else {
    LogFetchResult(GetChangePasswordUrlMetric::kNoUrlOverrideAvailable);
  }
  return GURL();
}

void AffiliationServiceImpl::OnFetchSucceeded(
    AffiliationFetcherInterface* fetcher,
    std::unique_ptr<AffiliationFetcherDelegate::Result> result) {
  auto processed_fetch =
      std::ranges::find(pending_fetches_, fetcher,
                        [](const auto& info) { return info.fetcher.get(); });
  if (processed_fetch == pending_fetches_.end())
    return;

  change_password_urls_[processed_fetch->requested_facet] =
      processed_fetch->GetChangePasswordURL(std::move(result));

  pending_fetches_.erase(processed_fetch);
}

void AffiliationServiceImpl::OnFetchFailed(
    AffiliationFetcherInterface* fetcher) {
  std::erase_if(pending_fetches_, [fetcher](const auto& info) {
    return info.fetcher.get() == fetcher;
  });
}

void AffiliationServiceImpl::OnMalformedResponse(
    AffiliationFetcherInterface* fetcher) {
  std::erase_if(pending_fetches_, [fetcher](const auto& info) {
    return info.fetcher.get() == fetcher;
  });
}

void AffiliationServiceImpl::GetAffiliationsAndBranding(
    const FacetURI& facet_uri,
    ResultCallback result_callback) {
  PostToBackend(&AffiliationBackend::GetAffiliationsAndBranding, facet_uri,
                StrategyOnCacheMiss::FAIL, std::move(result_callback),
                base::SequencedTaskRunner::GetCurrentDefault());
}

void AffiliationServiceImpl::Prefetch(const FacetURI& facet_uri,
                                      const base::Time& keep_fresh_until) {
  PostToBackend(&AffiliationBackend::Prefetch, facet_uri, keep_fresh_until);
}

void AffiliationServiceImpl::CancelPrefetch(
    const FacetURI& facet_uri,
    const base::Time& keep_fresh_until) {
  PostToBackend(&AffiliationBackend::CancelPrefetch, facet_uri,
                keep_fresh_until);
}

void AffiliationServiceImpl::KeepPrefetchForFacets(
    std::vector<FacetURI> facet_uris) {
  PostToBackend(&AffiliationBackend::KeepPrefetchForFacets,
                std::move(facet_uris));
}

void AffiliationServiceImpl::TrimUnusedCache(std::vector<FacetURI> facet_uris) {
  PostToBackend(&AffiliationBackend::TrimUnusedCache, std::move(facet_uris));
}

void AffiliationServiceImpl::GetGroupingInfo(std::vector<FacetURI> facet_uris,
                                             GroupsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If `backend` is destroyed there is nothing to do.
  if (!backend_) {
    return;
  }

  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AffiliationBackend::GetGroupingInfo,
                     base::Unretained(backend_.get()), std::move(facet_uris)),
      std::move(callback));
}

void AffiliationServiceImpl::GetPSLExtensions(
    base::OnceCallback<void(std::vector<std::string>)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If `backend` is destroyed there is nothing to do.
  if (!backend_) {
    return;
  }

  backend_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AffiliationBackend::GetPSLExtensions,
                     base::Unretained(backend_.get())),
      std::move(callback));
}

void AffiliationServiceImpl::UpdateAffiliationsAndBranding(
    const std::vector<FacetURI>& facets,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  auto callback_in_main_sequence =
      base::BindOnce(base::IgnoreResult(&base::TaskRunner::PostTask),
                     base::SequencedTaskRunner::GetCurrentDefault(), FROM_HERE,
                     std::move(callback));
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&AffiliationBackend::UpdateAffiliationsAndBranding,
                     base::Unretained(backend_.get()), facets,
                     std::move(callback_in_main_sequence)));
}

void AffiliationServiceImpl::RegisterSource(
    std::unique_ptr<AffiliationSource> source) {
  prefetcher_.RegisterSource(std::move(source));
}

}  // namespace affiliations
