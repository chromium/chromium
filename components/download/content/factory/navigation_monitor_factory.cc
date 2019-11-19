// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/factory/navigation_monitor_factory.h"

#include "components/download/internal/background_service/navigation_monitor_impl.h"
#include "components/keyed_service/core/simple_dependency_manager.h"

namespace download {

// static
NavigationMonitorFactory* NavigationMonitorFactory::GetInstance() {
  return base::Singleton<NavigationMonitorFactory>::get();
}

// static
download::NavigationMonitor* NavigationMonitorFactory::GetForKey(
    SimpleFactoryKey* key) {
  return static_cast<download::NavigationMonitor*>(
      GetInstance()->GetServiceForKey(key, true));
}

NavigationMonitorFactory::NavigationMonitorFactory()
    : SimpleKeyedServiceFactory("download::NavigationMonitor",
                                SimpleDependencyManager::GetInstance()) {}

NavigationMonitorFactory::~NavigationMonitorFactory() = default;

std::unique_ptr<KeyedService> NavigationMonitorFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  return std::make_unique<NavigationMonitorImpl>();
}

SimpleFactoryKey* NavigationMonitorFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  return key;
}

}  // namespace download
