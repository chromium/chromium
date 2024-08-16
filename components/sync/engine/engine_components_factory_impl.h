// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ENGINE_COMPONENTS_FACTORY_IMPL_H_
#define COMPONENTS_SYNC_ENGINE_ENGINE_COMPONENTS_FACTORY_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "components/sync/engine/engine_components_factory.h"

namespace syncer {

// An EngineComponentsFactory implementation designed for real production /
// normal use.
class EngineComponentsFactoryImpl : public EngineComponentsFactory {
 public:
  explicit EngineComponentsFactoryImpl(const Switches& switches);
  EngineComponentsFactoryImpl(const EngineComponentsFactoryImpl&) = delete;
  EngineComponentsFactoryImpl& operator=(const EngineComponentsFactoryImpl&) =
      delete;
  ~EngineComponentsFactoryImpl() override = default;

  std::unique_ptr<SyncScheduler> BuildScheduler(
      const std::string& name,
      SyncCycleContext* context,
      CancelationSignal* cancelation_signal,
      bool ignore_auth_credentials) override;

  std::unique_ptr<SyncCycleContext> BuildContext(
      ServerConnectionManager* connection_manager,
      ExtensionsActivity* extensions_activity,
      const std::vector<SyncEngineEventListener*>& listeners,
      DebugInfoGetter* debug_info_getter,
      DataTypeRegistry* data_type_registry,
      const std::string& cache_guid,
      const std::string& store_birthday,
      const std::string& bag_of_chips,
      base::TimeDelta poll_interval) override;

 private:
  const Switches switches_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ENGINE_COMPONENTS_FACTORY_IMPL_H_
