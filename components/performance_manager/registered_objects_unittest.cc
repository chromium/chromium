// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/registered_objects.h"

#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class Registered {
 public:
  Registered() = default;
  virtual ~Registered() = default;
  virtual uintptr_t GetTypeId() const = 0;
};

class Foo : public Registered {
 public:
  Foo() = default;
  ~Foo() override = default;
  static uintptr_t TypeId() { return 0; }
  uintptr_t GetTypeId() const override { return TypeId(); }
};

class Bar : public Registered {
 public:
  Bar() = default;
  ~Bar() override = default;
  static uintptr_t TypeId() { return 1; }
  uintptr_t GetTypeId() const override { return TypeId(); }
};

}  // namespace

TEST(RegisteredObjectsTest, ContainerWorksAsAdvertised) {
  std::unique_ptr<RegisteredObjects<Registered>> registry(
      new RegisteredObjects<Registered>());

  ASSERT_NE(Foo::TypeId(), Bar::TypeId());

  EXPECT_FALSE(registry->GetRegisteredObject(Foo::TypeId()));
  EXPECT_FALSE(registry->GetRegisteredObject(Bar::TypeId()));

  // Insertion works.
  Foo foo;
  EXPECT_EQ(0u, registry->size());
  registry->RegisterObject(&foo);
  EXPECT_EQ(1u, registry->size());
  EXPECT_EQ(&foo, registry->GetRegisteredObject(Foo::TypeId()));
  EXPECT_FALSE(registry->GetRegisteredObject(Bar::TypeId()));

  // Inserting again fails.
  EXPECT_CHECK_DEATH(registry->RegisterObject(&foo));

  // Unregistered the wrong object fails.
  Foo foo2;
  EXPECT_CHECK_DEATH(registry->UnregisterObject(&foo2));

  // Unregistering works.
  registry->UnregisterObject(&foo);
  EXPECT_EQ(0u, registry->size());
  EXPECT_FALSE(registry->GetRegisteredObject(Foo::TypeId()));
  EXPECT_FALSE(registry->GetRegisteredObject(Bar::TypeId()));

  // Unregistering again fails.
  EXPECT_CHECK_DEATH(registry->UnregisterObject(&foo));
  EXPECT_CHECK_DEATH(registry->UnregisterObject(&foo2));

  // Registering multiple objects works.
  Bar bar;
  registry->RegisterObject(&foo);
  EXPECT_EQ(1u, registry->size());
  registry->RegisterObject(&bar);
  EXPECT_EQ(2u, registry->size());
  EXPECT_EQ(&foo, registry->GetRegisteredObject(Foo::TypeId()));
  EXPECT_EQ(&bar, registry->GetRegisteredObject(Bar::TypeId()));

  // Expect the container to explode if deleted with objects.
  EXPECT_CHECK_DEATH(registry.reset());

  // Empty the registry.
  registry->UnregisterObject(&bar);
  EXPECT_EQ(1u, registry->size());
  registry->UnregisterObject(&foo);
  EXPECT_EQ(0u, registry->size());
}

}  // namespace performance_manager
