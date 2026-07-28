// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_handler_factory_registry_impl.h"

#include <vector>

#include "base/test/gtest_util.h"
#include "components/browser_actuator/public/transport_handler_factory.h"
#include "components/browser_actuator/test_support/mock_transport_handler_factory.h"
#include "components/browser_actuator/test_support/test_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {

TEST(TransportHandlerFactoryRegistryImplTest, IsEmpty) {
  TransportHandlerFactoryRegistryImpl registry;
  EXPECT_TRUE(registry.IsEmpty());

  MockTransportHandlerFactory factory({kTestPayloadTypeA});
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
  MockTransportHandlerFactory factory({kTestPayloadTypeA, kTestPayloadTypeB});

  registry.RegisterFactory(&factory);

  // Should register for both supported types.
  EXPECT_EQ(registry.GetFactories(kTestPayloadTypeA).size(), 1u);
  EXPECT_EQ(registry.GetFactories(kTestPayloadTypeB).size(), 1u);

  // Duplicate registration ignored.
  registry.RegisterFactory(&factory);
  EXPECT_EQ(registry.GetFactories(kTestPayloadTypeA).size(), 1u);
}

TEST(TransportHandlerFactoryRegistryImplTest, UnregisterFactory) {
  TransportHandlerFactoryRegistryImpl registry;
  MockTransportHandlerFactory factory1({kTestPayloadTypeA, kTestPayloadTypeB},
                                       kTestFactoryId1);
  MockTransportHandlerFactory factory2({kTestPayloadTypeB}, kTestFactoryId2);

  registry.RegisterFactory(&factory1);
  registry.RegisterFactory(&factory2);

  registry.UnregisterFactory(&factory1);

  // factory1 should be removed from all types.
  EXPECT_TRUE(registry.GetFactories(kTestPayloadTypeA).empty());
  std::vector<TransportHandlerFactory*> factories_b =
      registry.GetFactories(kTestPayloadTypeB);
  EXPECT_EQ(factories_b.size(), 1u);
  EXPECT_EQ(factories_b[0], &factory2);

  // Unregistering a factory that is not registered is a no-op.
  MockTransportHandlerFactory factory3({kTestPayloadTypeC}, kTestFactoryId3);
  registry.UnregisterFactory(&factory3);
  EXPECT_TRUE(registry.GetFactories(kTestPayloadTypeC).empty());
}

TEST(TransportHandlerFactoryRegistryImplTest, GetFactories) {
  TransportHandlerFactoryRegistryImpl registry;
  MockTransportHandlerFactory factory1({kTestPayloadTypeA, kTestPayloadTypeB},
                                       kTestFactoryId1);
  MockTransportHandlerFactory factory2({kTestPayloadTypeB, kTestPayloadTypeC},
                                       kTestFactoryId2);

  registry.RegisterFactory(&factory1);
  registry.RegisterFactory(&factory2);

  std::vector<TransportHandlerFactory*> factories_a =
      registry.GetFactories(kTestPayloadTypeA);
  EXPECT_EQ(factories_a.size(), 1u);
  EXPECT_EQ(factories_a[0], &factory1);

  // GetFactories returns factories in registration order
  std::vector<TransportHandlerFactory*> factories_b =
      registry.GetFactories(kTestPayloadTypeB);
  EXPECT_EQ(factories_b.size(), 2u);
  EXPECT_EQ(factories_b[0], &factory1);
  EXPECT_EQ(factories_b[1], &factory2);

  // Get factories for an unregistered type should return empty vector.
  EXPECT_TRUE(registry.GetFactories(static_cast<PayloadType>(99)).empty());
}

TEST(TransportHandlerFactoryRegistryImplTest, Clear) {
  TransportHandlerFactoryRegistryImpl registry;
  MockTransportHandlerFactory factory1({kTestPayloadTypeA}, kTestFactoryId1);
  MockTransportHandlerFactory factory2({kTestPayloadTypeB}, kTestFactoryId2);

  registry.RegisterFactory(&factory1);
  registry.RegisterFactory(&factory2);

  registry.Clear();

  EXPECT_TRUE(registry.IsEmpty());
  EXPECT_TRUE(registry.GetFactories(kTestPayloadTypeA).empty());
  EXPECT_TRUE(registry.GetFactories(kTestPayloadTypeB).empty());
}

TEST(TransportHandlerFactoryRegistryImplTest,
     RegisterDuplicateFactoryIdCrashes) {
  TransportHandlerFactoryRegistryImpl registry;
  MockTransportHandlerFactory factory1({kTestPayloadTypeA}, kTestFactoryId1);
  MockTransportHandlerFactory factory2({kTestPayloadTypeB}, kTestFactoryId1);

  registry.RegisterFactory(&factory1);
  EXPECT_DCHECK_DEATH(registry.RegisterFactory(&factory2));
}

}  // namespace browser_actuator
