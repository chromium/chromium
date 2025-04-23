// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CONFIGURE_REASON_H_
#define COMPONENTS_SYNC_ENGINE_CONFIGURE_REASON_H_

namespace syncer {

// Enum representing the reason that triggered a configuration cycle, i.e.
// initializing DataTypeManager or changing the set of datatypes that should be
// actively syncing. Note that configuration cycles can involve downloading
// updates from the server (e.g. fully disabled datatype is now enabled) but
// it's not always the case (in particular during browser startup).
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

  // An existing client restarted sync, including cases like profile startup
  // or a persistent auth error having been fixed. In many cases, specially
  // for the profile startup case, these configuration requests don't
  // actually lead to downloading updates from the server.
  CONFIGURE_REASON_EXISTING_CLIENT_RESTART,

  // A configuration due to enabling or disabling encrypted types due to
  // cryptographer errors/resolutions.
  CONFIGURE_REASON_CRYPTO,

  // The client is configuring because of a programmatic type enable/disable,
  // such as when an error is encountered/resolved.
  CONFIGURE_REASON_PROGRAMMATIC,
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CONFIGURE_REASON_H_
