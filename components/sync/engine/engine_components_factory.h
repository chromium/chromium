// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ENGINE_COMPONENTS_FACTORY_H_
#define COMPONENTS_SYNC_ENGINE_ENGINE_COMPONENTS_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"

namespace syncer {

class CancelationSignal;
class DebugInfoGetter;
class ExtensionsActivity;
class DataTypeRegistry;
class ServerConnectionManager;
class SyncCycleContext;
class SyncEngineEventListener;
class SyncScheduler;

// EngineComponentsFactory exists so that tests can override creation of
// components used by the SyncManager and other things inside engine/.
class EngineComponentsFactory {
 public:
  enum BackoffOverride {
    BACKOFF_NORMAL,
    // Use this value for integration testing to avoid long delays /
    // timing out tests. Uses kInitialBackoffShortRetrySeconds (see
    // polling_constants.h) for all initial retries.
    BACKOFF_SHORT_INITIAL_RETRY_OVERRIDE
  };

  // Configuration options for internal components. This struct is expected
  // to grow and shrink over time with transient features / experiments,
  // roughly following command line flags in chrome. Implementations of
  // EngineComponentsFactory can use this information to build components
  // with appropriate bells and whistles.
  struct Switches {
    BackoffOverride backoff_override;
    bool force_short_nudge_delay_for_test;
  };

  virtual ~EngineComponentsFactory() = default;

  virtual std::unique_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      SyncCycleContext* context,
      CancelationSignal* cancelation_signal,
      bool ignore_auth_credentials) = 0;

  virtual std::unique_ptr<SyncCycleContext> BuildContext(
      ServerConnectionManager* connection_manager,
      ExtensionsActivity* extensions_activity,
      const std::vector<SyncEngineEventListener*>& listeners,
      DebugInfoGetter* debug_info_getter,
      DataTypeRegistry* data_type_registry,
      const std::string& cache_guid,
      const std::string& store_birthday,
      const std::string& bag_of_chips,
      base::TimeDelta poll_interval) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ENGINE_COMPONENTS_FACTORY_H_
