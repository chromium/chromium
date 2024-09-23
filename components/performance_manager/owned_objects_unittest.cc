// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/owned_objects.h"

#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace {

class Owned {
 public:
  Owned() = default;
  virtual ~Owned() { OnDestructor(); }

  Owned(const Owned&) = delete;
  Owned& operator=(const Owned) = delete;

  MOCK_METHOD(void, OnPassedTo, (void*));
  MOCK_METHOD(void, OnTakenFrom, (void*));
  MOCK_METHOD(void, OnDestructor, ());
};

}  // namespace

TEST(OwnedObjectsTest, ContainerWorksAsAdvertised) {
  using Owner =
      OwnedObjects<Owned, void*, &Owned::OnPassedTo, &Owned::OnTakenFrom>;
  std::unique_ptr<Owner> owner = std::make_unique<Owner>();

  std::unique_ptr<Owned> owned1 = std::make_unique<Owned>();
  std::unique_ptr<Owned> owned2 = std::make_unique<Owned>();
  auto* raw1 = owned1.get();
  auto* raw2 = owned2.get();

  // Pass both objects to the owner.
  EXPECT_EQ(0u, owner->size());
  EXPECT_CALL(*raw1, OnPassedTo(owner.get()));
  owner->PassObject(std::move(owned1), owner.get());
  EXPECT_EQ(1u, owner->size());
  EXPECT_CALL(*raw2, OnPassedTo(owner.get()));
  owner->PassObject(std::move(owned2), owner.get());
  EXPECT_EQ(2u, owner->size());

  // Take one back.
  EXPECT_CALL(*raw1, OnTakenFrom(owner.get()));
  owned1 = owner->TakeObject(raw1, owner.get());
  EXPECT_EQ(1u, owner->size());

  // Destroy that object and expect its destructor to have been invoked.
  EXPECT_CALL(*raw1, OnDestructor());
  owned1.reset();
  raw1 = nullptr;

  // Expect the container to explode if deleted with objects.
  EXPECT_CHECK_DEATH(owner.reset());

  // Ask the container to release the remaining objects.
  EXPECT_CALL(*raw2, OnTakenFrom(owner.get()));
  EXPECT_CALL(*raw2, OnDestructor());
  owner->ReleaseObjects(owner.get());
  raw2 = nullptr;

  // Destroying the container is now safe.
  owner.reset();
}

}  // namespace performance_manager
