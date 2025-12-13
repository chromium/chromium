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
// LINT.IfChange(ConfigureReason)
enum class ConfigureReason {
  // We should never be here during actual configure. This is for setting
  // default values.
  kUnknown = 0,

  // The client is configuring because the user opted to sync a different set
  // of datatypes.
  kReconfiguration = 1,

  // The client is configuring because the client is being asked to migrate.
  kMigration = 2,

  // Setting up sync performs an initial config to download NIGORI data, and
  // also a config to download initial data once the user selects types.
  kNewClient = 3,

  // An existing client restarted sync, including cases like profile startup
  // or a persistent auth error having been fixed. In many cases, specially
  // for the profile startup case, these configuration requests don't
  // actually lead to downloading updates from the server.
  kExistingClientRestart = 4,

  // A configuration due to enabling or disabling encrypted types due to
  // cryptographer errors/resolutions.
  kCrypto = 5,

  // The client is configuring because of a programmatic type enable/disable,
  // such as when an error is encountered/resolved.
  kProgrammatic = 6,

  kMaxValue = kProgrammatic,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/sync/enums.xml:SyncConfigureDataTypeManagerReason)

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CONFIGURE_REASON_H_
