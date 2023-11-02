// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/model_neutral_state.h"

namespace syncer {

ModelNeutralState::ModelNeutralState()
    : num_successful_commits(0),
      num_successful_bookmark_commits(0),
      num_updates_downloaded_total(0),
      num_tombstone_updates_downloaded_total(0),
      num_server_conflicts(0),
      items_committed(false) {}

ModelNeutralState::ModelNeutralState(const ModelNeutralState& other) = default;

ModelNeutralState::~ModelNeutralState() = default;

bool HasSyncerError(const ModelNeutralState& state) {
  const bool get_key_error = state.last_get_key_result.IsActualError();
  const bool download_updates_error =
      state.last_download_updates_result.IsActualError();
  const bool commit_error = state.commit_result.IsActualError();
  return get_key_error || download_updates_error || commit_error;
}

}  // namespace syncer
