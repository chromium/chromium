// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"

#include <optional>

#include "base/functional/callback.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace affiliations {

DomainRelationChecker::DomainRelationChecker(
    AffiliationService& affiliation_service)
    : affiliation_service_(affiliation_service) {}

DomainRelationChecker::~DomainRelationChecker() = default;

void DomainRelationChecker::Check(
    const url::Origin& origin_1,
    const url::Origin& origin_2,
    base::OnceCallback<void(std::optional<MatchType>)> result_cb) {
  // TODO(crbug.com/504573041): Implement matching checks.
  std::move(result_cb).Run(std::nullopt);
}

void DomainRelationChecker::Check(
    const GURL& url_1,
    const GURL& url_2,
    base::OnceCallback<void(std::optional<MatchType>)> result_cb) {
  Check(url::Origin::Create(url_1), url::Origin::Create(url_2),
        std::move(result_cb));
}

}  // namespace affiliations
