// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_INSTANCE_WAITER_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_INSTANCE_WAITER_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/instance_registry.h"

namespace apps {

// Waits for the given app to reach the given state in the given
// InstanceRegistry. Makes the simplifying assumption that there exists at most
// one instance of the app (guarded by CHECKs).
class AppInstanceWaiter : public apps::InstanceRegistry::Observer {
 public:
  AppInstanceWaiter(apps::InstanceRegistry& registry,
                    const std::string& app_id,
                    apps::InstanceState state);
  ~AppInstanceWaiter() override;

  void Await();

  void OnInstanceUpdate(const apps::InstanceUpdate& update) override;

  void OnInstanceRegistryWillBeDestroyed(
      apps::InstanceRegistry* cache) override;

 private:
  const raw_ref<apps::InstanceRegistry> registry_;
  const std::string app_id_;
  const apps::InstanceState state_;
  base::RunLoop run_loop_;
  base::ScopedObservation<apps::InstanceRegistry,
                          apps::InstanceRegistry::Observer>
      observation_{this};
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_INSTANCE_WAITER_H_
