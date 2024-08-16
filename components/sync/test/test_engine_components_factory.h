// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_TEST_ENGINE_COMPONENTS_FACTORY_H_
#define COMPONENTS_SYNC_TEST_TEST_ENGINE_COMPONENTS_FACTORY_H_

#include <memory>
#include <string>
#include <vector>

#include "components/sync/engine/engine_components_factory.h"

namespace syncer {

class TestEngineComponentsFactory : public EngineComponentsFactory {
 public:
  TestEngineComponentsFactory() = default;
  TestEngineComponentsFactory(const TestEngineComponentsFactory&) = delete;
  TestEngineComponentsFactory& operator=(const TestEngineComponentsFactory&) =
      delete;
  ~TestEngineComponentsFactory() override = default;

  std::unique_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      SyncCycleContext* context,
      CancelationSignal* cancelation_signal,
      bool ignore_auth_credentials) override;

  std::unique_ptr<SyncCycleContext> BuildContext(
      ServerConnectionManager* connection_manager,
      ExtensionsActivity* monitor,
      const std::vector<SyncEngineEventListener*>& listeners,
      DebugInfoGetter* debug_info_getter,
      DataTypeRegistry* data_type_registry,
      const std::string& cache_guid,
      const std::string& store_birthday,
      const std::string& bag_of_chips,
      base::TimeDelta poll_interval) override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_TEST_ENGINE_COMPONENTS_FACTORY_H_
