// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_CONFIGURE_CONTEXT_H_
#define COMPONENTS_SYNC_DRIVER_CONFIGURE_CONTEXT_H_

#include <string>

#include "components/sync/base/sync_mode.h"
#include "components/sync/engine/configure_reason.h"
#include "google_apis/gaia/core_account_id.h"

namespace syncer {

// Struct describing in which context sync was enabled, including state that can
// be assumed to be fixed while sync is enabled (or, more precisely, is
// representative of the last (re)configuration request). It's built by
// ProfileSyncService and plumbed through DataTypeManager until datatype
// controllers, which for USS datatypes propagate analogous information to the
// processor/bridge via DataTypeActivationRequest.
struct ConfigureContext {
  CoreAccountId authenticated_account_id;
  std::string cache_guid;
  SyncMode sync_mode = SyncMode::kFull;
  ConfigureReason reason = CONFIGURE_REASON_UNKNOWN;
  base::Time configuration_start_time;
  // TODO(mastiz): Consider adding |requested_types| here, but currently there
  // are subtle differences across layers (e.g. where control types are
  // enforced).
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_CONFIGURE_CONTEXT_H_
