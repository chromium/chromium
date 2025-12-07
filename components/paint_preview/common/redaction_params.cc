// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/redaction_params.h"

#include "base/containers/contains.h"
#include "net/base/schemeful_site.h"

namespace paint_preview {

RedactionParams::RedactionParams() = default;

RedactionParams::RedactionParams(
    base::flat_set<url::Origin> allowed_origins,
    base::flat_set<net::SchemefulSite> allowed_sites)
    : state_({std::move(allowed_origins), std::move(allowed_sites)}) {}

RedactionParams::RedactionParams(const RedactionParams&) = default;
RedactionParams& RedactionParams::operator=(const RedactionParams&) = default;
RedactionParams::RedactionParams(RedactionParams&&) = default;
RedactionParams& RedactionParams::operator=(RedactionParams&&) = default;

RedactionParams::~RedactionParams() = default;

RedactionParams::State::State(base::flat_set<url::Origin> allowed_origins,
                              base::flat_set<net::SchemefulSite> allowed_sites)
    : allowed_origins(std::move(allowed_origins)),
      allowed_sites(std::move(allowed_sites)) {}

RedactionParams::State::State(const RedactionParams::State&) = default;
RedactionParams::State& RedactionParams::State::operator=(
    const RedactionParams::State&) = default;
RedactionParams::State::State(RedactionParams::State&&) = default;
RedactionParams::State& RedactionParams::State::operator=(
    RedactionParams::State&&) = default;

RedactionParams::State::~State() = default;

bool RedactionParams::ShouldRedactSubframe(
    const url::Origin& frame_origin) const {
  if (!state_) {
    return false;
  }
  return !state_->AllowlistContainsOrigin(frame_origin) &&
         !state_->AllowlistContainsSite(frame_origin);
}

bool RedactionParams::State::AllowlistContainsOrigin(
    const url::Origin& origin) const {
  return base::Contains(allowed_origins, origin);
}

bool RedactionParams::State::AllowlistContainsSite(
    const url::Origin& origin) const {
  return !allowed_sites.empty() &&
         base::Contains(allowed_sites, net::SchemefulSite(origin));
}

}  // namespace paint_preview
