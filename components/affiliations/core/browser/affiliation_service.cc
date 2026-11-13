// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_service.h"

#include <utility>
#include <vector>

#include "url/gurl.h"

namespace affiliations {

AffiliationService::ChangePasswordUrlMatch::ChangePasswordUrlMatch(
    GURL change_password_url,
    std::vector<FacetURI> affiliations)
    : change_password_url(std::move(change_password_url)),
      affiliations(std::move(affiliations)) {}

AffiliationService::ChangePasswordUrlMatch::ChangePasswordUrlMatch(
    const ChangePasswordUrlMatch&) = default;

AffiliationService::ChangePasswordUrlMatch::ChangePasswordUrlMatch(
    ChangePasswordUrlMatch&&) = default;

AffiliationService::ChangePasswordUrlMatch&
AffiliationService::ChangePasswordUrlMatch::operator=(
    const ChangePasswordUrlMatch&) = default;

AffiliationService::ChangePasswordUrlMatch&
AffiliationService::ChangePasswordUrlMatch::operator=(
    ChangePasswordUrlMatch&&) = default;

AffiliationService::ChangePasswordUrlMatch::~ChangePasswordUrlMatch() = default;

}  // namespace affiliations
