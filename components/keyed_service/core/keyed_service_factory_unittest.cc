// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/keyed_service_factory.h"

#include <memory>

#include "components/keyed_service/core/dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Exposes the protected context lifecycle methods for testing.
class TestDependencyManager : public DependencyManager {
 public:
  TestDependencyManager() = default;
  ~TestDependencyManager() override = default;

  using DependencyManager::CreateContextServices;
  using DependencyManager::DestroyContextServices;
  using DependencyManager::DisableContextServicesByDefault;
  using DependencyManager::MarkContextLive;
};

class TestService : public KeyedService {};

class TestFactory : public KeyedServiceFactory {
 public:
  explicit TestFactory(DependencyManager* manager)
      : KeyedServiceFactory("TestFactory", manager, SIMPLE) {}

  // Allows the service even for contexts whose services are disabled by
  // default.
  void set_allowed_when_services_disabled_by_default(bool allowed) {
    allowed_when_services_disabled_by_default_ = allowed;
  }

  KeyedService* GetForContext(void* context) {
    return GetServiceForContext(context, /*created=*/true);
  }

 private:
  // KeyedServiceFactory:
  void* GetContextToUse(void* context) const override { return context; }
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      void* context) const override {
    return std::make_unique<TestService>();
  }
  bool ServiceIsAllowedWhenServicesDisabledByDefault() const override {
    return allowed_when_services_disabled_by_default_;
  }

  bool allowed_when_services_disabled_by_default_ = false;
};

class KeyedServiceFactoryDisabledByDefaultTest : public testing::Test {
 protected:
  void CreateContext(void* context, bool services_disabled_by_default) {
    manager_.MarkContextLive(context);
    if (services_disabled_by_default) {
      manager_.DisableContextServicesByDefault(context);
    }
    manager_.CreateContextServices(context, /*is_testing_context=*/false);
  }

  void DestroyContext(void* context) {
    manager_.DestroyContextServices(context);
  }

  TestDependencyManager& manager() { return manager_; }

 private:
  TestDependencyManager manager_;
};

}  // namespace

TEST_F(KeyedServiceFactoryDisabledByDefaultTest, ServiceIsCreatedByDefault) {
  TestFactory factory(&manager());
  int context = 0;
  CreateContext(&context, /*services_disabled_by_default=*/false);

  EXPECT_TRUE(factory.GetForContext(&context));

  DestroyContext(&context);
}

TEST_F(KeyedServiceFactoryDisabledByDefaultTest,
       ServiceIsNotCreatedWhenServicesDisabledByDefault) {
  TestFactory factory(&manager());
  int context = 0;
  CreateContext(&context, /*services_disabled_by_default=*/true);

  EXPECT_FALSE(factory.GetForContext(&context));

  DestroyContext(&context);
}

TEST_F(KeyedServiceFactoryDisabledByDefaultTest,
       ServiceIsCreatedWhenAllowedByFactory) {
  TestFactory factory(&manager());
  factory.set_allowed_when_services_disabled_by_default(true);
  int context = 0;
  CreateContext(&context, /*services_disabled_by_default=*/true);

  EXPECT_TRUE(factory.GetForContext(&context));

  DestroyContext(&context);
}

TEST_F(KeyedServiceFactoryDisabledByDefaultTest, OnlyAppliesToGivenContext) {
  TestFactory factory(&manager());
  int disabled_context = 0;
  int other_context = 0;
  CreateContext(&disabled_context, /*services_disabled_by_default=*/true);
  CreateContext(&other_context, /*services_disabled_by_default=*/false);

  EXPECT_FALSE(factory.GetForContext(&disabled_context));
  EXPECT_TRUE(factory.GetForContext(&other_context));

  DestroyContext(&disabled_context);
  DestroyContext(&other_context);
}

TEST_F(KeyedServiceFactoryDisabledByDefaultTest,
       IsClearedWhenContextIsDestroyed) {
  TestFactory factory(&manager());
  int context = 0;
  CreateContext(&context, /*services_disabled_by_default=*/true);
  EXPECT_FALSE(factory.GetForContext(&context));
  DestroyContext(&context);

  // A new context created at the same address has services enabled.
  CreateContext(&context, /*services_disabled_by_default=*/false);
  EXPECT_TRUE(factory.GetForContext(&context));
  DestroyContext(&context);
}
