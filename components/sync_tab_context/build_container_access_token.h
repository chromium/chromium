// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_BUILD_CONTAINER_ACCESS_TOKEN_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_BUILD_CONTAINER_ACCESS_TOKEN_H_

#include <optional>
#include <string>

#include "components/sync_tab_context/ephemeral_key_fetcher.h"

namespace syncer {
class AgileSymmetricKeySet;
}  // namespace syncer

namespace sync_tab_context {

// Builds a serialized `sync_pb::TabContextContainerAccessToken` by encrypting
// `container_key_set` with `ephemeral_key_result.ephemeral_key`. Returns
// `std::nullopt` if encryption fails.
std::optional<std::string> BuildContainerAccessToken(
    const syncer::AgileSymmetricKeySet& container_key_set,
    EphemeralKeyFetcher::Result ephemeral_key_result);

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_BUILD_CONTAINER_ACCESS_TOKEN_H_
