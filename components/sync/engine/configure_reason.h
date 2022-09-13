// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CONFIGURE_REASON_H_
#define COMPONENTS_SYNC_ENGINE_CONFIGURE_REASON_H_

namespace syncer {

// Note: This should confirm with the enums in sync.proto for
// GetUpdatesCallerInfo. They will have 1:1 mapping but this will only map
// to a subset of the GetUpdatesCallerInfo enum values.
enum ConfigureReason {
  // We should never be here during actual configure. This is for setting
  // default values.
  CONFIGURE_REASON_UNKNOWN,

  // The client is configuring because the user opted to sync a different set
  // of datatypes.
  CONFIGURE_REASON_RECONFIGURATION,

  // The client is configuring because the client is being asked to migrate.
  CONFIGURE_REASON_MIGRATION,

  // Setting up sync performs an initial config to download NIGORI data, and
  // also a config to download initial data once the user selects types.
  CONFIGURE_REASON_NEW_CLIENT,

  // A new datatype is enabled for syncing due to a client upgrade.
  CONFIGURE_REASON_NEWLY_ENABLED_DATA_TYPE,

  // A configuration due to enabling or disabling encrypted types due to
  // cryptographer errors/resolutions.
  CONFIGURE_REASON_CRYPTO,

  // The client is configuring because of a programmatic type enable/disable,
  // such as when an error is encountered/resolved.
  CONFIGURE_REASON_PROGRAMMATIC,
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CONFIGURE_REASON_H_
