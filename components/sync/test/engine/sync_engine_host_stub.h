// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_ENGINE_SYNC_ENGINE_HOST_STUB_H_
#define COMPONENTS_SYNC_TEST_ENGINE_SYNC_ENGINE_HOST_STUB_H_

#include <string>

#include "components/sync/engine/sync_engine_host.h"

namespace syncer {

class SyncEngineHostStub : public SyncEngineHost {
 public:
  SyncEngineHostStub();
  ~SyncEngineHostStub() override;

  // SyncEngineHost implementation.
  void OnEngineInitialized(
      ModelTypeSet initial_types,
      const WeakHandle<JsBackend>& js_backend,
      const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
      const std::string& birthday,
      const std::string& bag_of_chips,
      bool success) override;
  void OnSyncCycleCompleted(const SyncCycleSnapshot& snapshot) override;
  void OnProtocolEvent(const ProtocolEvent& event) override;
  void OnConnectionStatusChange(ConnectionStatus status) override;
  void OnMigrationNeededForTypes(ModelTypeSet types) override;
  void OnActionableError(const SyncProtocolError& error) override;
  void OnBackedOffTypesChanged() override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_ENGINE_SYNC_ENGINE_HOST_STUB_H_
