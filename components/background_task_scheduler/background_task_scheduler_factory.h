// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_TASK_SCHEDULER_BACKGROUND_TASK_SCHEDULER_FACTORY_H_
#define COMPONENTS_BACKGROUND_TASK_SCHEDULER_BACKGROUND_TASK_SCHEDULER_FACTORY_H_

#include <memory>

#include "components/keyed_service/core/simple_keyed_service_factory.h"

class SimpleFactoryKey;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace background_task {
class BackgroundTaskScheduler;

// A factory for creating BackgroundTaskScheduler.
class BackgroundTaskSchedulerFactory : public SimpleKeyedServiceFactory {
 public:
  // Returns singleton instance of BackgroundTaskSchedulerFactory.
  static BackgroundTaskSchedulerFactory* GetInstance();

  // Returns the BackgroundTaskScheuler associated with |key|.
  static BackgroundTaskScheduler* GetForKey(SimpleFactoryKey* key);

  BackgroundTaskSchedulerFactory(const BackgroundTaskSchedulerFactory&) =
      delete;
  BackgroundTaskSchedulerFactory& operator=(
      const BackgroundTaskSchedulerFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<BackgroundTaskSchedulerFactory>;

  BackgroundTaskSchedulerFactory();
  ~BackgroundTaskSchedulerFactory() override;

  // SimpleKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

}  // namespace background_task

#endif  // COMPONENTS_BACKGROUND_TASK_SCHEDULER_BACKGROUND_TASK_SCHEDULER_FACTORY_H_
