// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/fake_affiliation_service.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/affiliations/core/browser/affiliation_utils.h"

namespace affiliations {

FakeAffiliationService::FakeAffiliationService() = default;

FakeAffiliationService::FakeAffiliationService(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {}

FakeAffiliationService::~FakeAffiliationService() = default;

const scoped_refptr<base::SequencedTaskRunner>&
FakeAffiliationService::GetTaskRunner() const {
  return task_runner_ ? task_runner_
                      : base::SequencedTaskRunner::GetCurrentDefault();
}

void FakeAffiliationService::AddAffiliationGroup(const AffiliatedFacets& group,
                                                 bool add_to_cache) {
  affiliation_groups_.push_back(group);
  if (add_to_cache) {
    for (const Facet& facet : group) {
      cache_.insert(facet.uri);
    }
  }
}

void FakeAffiliationService::AddGroupedFacets(const GroupedFacets& group,
                                              bool add_to_cache) {
  grouped_facets_.push_back(group);
  if (add_to_cache) {
    for (const Facet& facet : group.facets) {
      cache_.insert(facet.uri);
    }
  }
}

void FakeAffiliationService::SetPSLExtensions(
    std::vector<std::string> psl_extensions) {
  psl_extensions_ = std::move(psl_extensions);
}

void FakeAffiliationService::FetchChangePasswordURL(
    const GURL& url,
    base::OnceCallback<void(GURL)> callback) {
  GetTaskRunner()->PostTask(FROM_HERE,
                            base::BindOnce(std::move(callback), GURL()));
}

GURL FakeAffiliationService::GetChangePasswordURL(const GURL& url) const {
  return GURL();
}

void FakeAffiliationService::GetAffiliationsAndBranding(
    const FacetURI& facet_uri,
    ResultCallback result_callback) {
  if (!cache_.contains(facet_uri)) {
    GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_callback),
                                  AffiliatedFacets(), /*success=*/false));
    return;
  }

  for (const AffiliatedFacets& group : affiliation_groups_) {
    if (std::ranges::any_of(group, [&](const Facet& facet) {
          return facet.uri == facet_uri;
        })) {
      GetTaskRunner()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(result_callback), group, /*success=*/true));
      return;
    }
  }

  // If no match found in seeded groups, guarantee self hit by returning a
  // single-element group containing the queried facet itself.
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback),
                                AffiliatedFacets{Facet(
                                    facet_uri, FacetBrandingInfo(), GURL())},
                                /*success=*/true));
}

void FakeAffiliationService::Prefetch(const FacetURI& facet_uri,
                                      const base::Time& keep_fresh_until) {}

void FakeAffiliationService::CancelPrefetch(
    const FacetURI& facet_uri,
    const base::Time& keep_fresh_until) {}

void FakeAffiliationService::KeepPrefetchForFacets(
    std::vector<FacetURI> facet_uris) {}

void FakeAffiliationService::TrimUnusedCache(std::vector<FacetURI> facet_uris) {
}

void FakeAffiliationService::GetGroupingInfo(std::vector<FacetURI> facet_uris,
                                             GroupsCallback callback) {
  std::vector<GroupedFacets> result;
  result.reserve(facet_uris.size());
  for (const FacetURI& facet_uri : facet_uris) {
    GroupedFacets matched_group;
    matched_group.facets.push_back(Facet(facet_uri));

    for (const GroupedFacets& group : grouped_facets_) {
      if (std::ranges::any_of(group.facets, [&](const Facet& facet) {
            return facet.uri == facet_uri;
          })) {
        matched_group = group;
        break;
      }
    }
    result.push_back(std::move(matched_group));
  }
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

void FakeAffiliationService::GetPSLExtensions(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), psl_extensions_));
}

void FakeAffiliationService::UpdateAffiliationsAndBranding(
    const std::vector<FacetURI>& facets,
    base::OnceClosure callback) {
  for (const auto& facet : facets) {
    cache_.insert(facet);
  }
  GetTaskRunner()->PostTask(FROM_HERE, std::move(callback));
}

void FakeAffiliationService::RegisterSource(
    std::unique_ptr<AffiliationSource> source) {}

base::WeakPtr<AffiliationService> FakeAffiliationService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace affiliations
