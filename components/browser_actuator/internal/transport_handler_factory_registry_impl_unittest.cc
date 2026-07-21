// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_handler_factory_registry_impl.h"

#include <vector>

#include "components/browser_actuator/public/transport_handler_factory.h"
#include "components/browser_actuator/test_support/mock_transport_handler_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {

namespace {

constexpr PayloadType kTypeA = static_cast<PayloadType>(1);
constexpr PayloadType kTypeB = static_cast<PayloadType>(2);
constexpr PayloadType kTypeC = static_cast<PayloadType>(3);

}  // namespace

TEST(TransportHandlerFactoryRegistryImplTest, IsEmpty) {
  TransportHandlerFactoryRegistryImpl registry;
  EXPECT_TRUE(registry.IsEmpty());

  MockTransportHandlerFactory factory({kTypeA});
  registry.RegisterFactory(&factory);
  EXPECT_FALSE(registry.IsEmpty());

  registry.UnregisterFactory(&factory);
  EXPECT_TRUE(registry.IsEmpty());

  registry.RegisterFactory(&factory);
  registry.Clear();
  EXPECT_TRUE(registry.IsEmpty());
}

TEST(TransportHandlerFactoryRegistryImplTest, RegisterFactory) {
  TransportHandlerFactoryRegistryImpl registry;
  MockTransportHandlerFactory factory({kTypeA, kTypeB});

  registry.RegisterFactory(&factory);

  // Should register for both supported types.
  EXPECT_EQ(registry.GetFactories(kTypeA).size(), 1u);
  EXPECT_EQ(registry.GetFactories(kTypeB).size(), 1u);

  // Duplicate registration ignored.
  registry.RegisterFactory(&factory);
  EXPECT_EQ(registry.GetFactories(kTypeA).size(), 1u);
}

TEST(TransportHandlerFactoryRegistryImplTest, UnregisterFactory) {
  TransportHandlerFactoryRegistryImpl registry;
  MockTransportHandlerFactory factory1({kTypeA, kTypeB});
  MockTransportHandlerFactory factory2({kTypeB});

  registry.RegisterFactory(&factory1);
  registry.RegisterFactory(&factory2);

  registry.UnregisterFactory(&factory1);

  // factory1 should be removed from all types.
  EXPECT_TRUE(registry.GetFactories(kTypeA).empty());
  std::vector<TransportHandlerFactory*> factories_b =
      registry.GetFactories(kTypeB);
  EXPECT_EQ(factories_b.size(), 1u);
  EXPECT_EQ(factories_b[0], &factory2);

  // Unregistering a factory that is not registered is a no-op.
  MockTransportHandlerFactory factory3({kTypeC});
  registry.UnregisterFactory(&factory3);
  EXPECT_TRUE(registry.GetFactories(kTypeC).empty());
}

TEST(TransportHandlerFactoryRegistryImplTest, GetFactories) {
  TransportHandlerFactoryRegistryImpl registry;
  MockTransportHandlerFactory factory1({kTypeA, kTypeB});
  MockTransportHandlerFactory factory2({kTypeB, kTypeC});

  registry.RegisterFactory(&factory1);
  registry.RegisterFactory(&factory2);

  std::vector<TransportHandlerFactory*> factories_a =
      registry.GetFactories(kTypeA);
  EXPECT_EQ(factories_a.size(), 1u);
  EXPECT_EQ(factories_a[0], &factory1);

  // GetFactories returns factories in registration order
  std::vector<TransportHandlerFactory*> factories_b =
      registry.GetFactories(kTypeB);
  EXPECT_EQ(factories_b.size(), 2u);
  EXPECT_EQ(factories_b[0], &factory1);
  EXPECT_EQ(factories_b[1], &factory2);

  // Get factories for an unregistered type should return empty vector.
  EXPECT_TRUE(registry.GetFactories(static_cast<PayloadType>(99)).empty());
}

TEST(TransportHandlerFactoryRegistryImplTest, Clear) {
  TransportHandlerFactoryRegistryImpl registry;
  MockTransportHandlerFactory factory1({kTypeA});
  MockTransportHandlerFactory factory2({kTypeB});

  registry.RegisterFactory(&factory1);
  registry.RegisterFactory(&factory2);

  registry.Clear();

  EXPECT_TRUE(registry.IsEmpty());
  EXPECT_TRUE(registry.GetFactories(kTypeA).empty());
  EXPECT_TRUE(registry.GetFactories(kTypeB).empty());
}

}  // namespace browser_actuator
