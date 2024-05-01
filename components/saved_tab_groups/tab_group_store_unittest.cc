// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_store.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/saved_tab_group_test_utils.h"
#include "components/saved_tab_groups/tab_group_store.h"
#include "components/saved_tab_groups/tab_group_store_delegate.h"
#include "components/saved_tab_groups/tab_group_store_id.h"
#include "components/saved_tab_groups/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;

namespace tab_groups {
namespace {

class FakeTabGroupStoreDelegate : public TabGroupStoreDelegate {
 public:
  FakeTabGroupStoreDelegate() = default;
  ~FakeTabGroupStoreDelegate() override = default;

  // TabGroupStoreDelegate implementation.
  void GetAllTabGroupIDMetadatas(GetCallback callback) override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), cache_));
  }

  void StoreTabGroupIDMetadata(
      const base::Uuid& sync_guid,
      const TabGroupIDMetadata& tab_group_id_metadata) override {
    cache_.emplace(std::pair(sync_guid, tab_group_id_metadata));
  }

  void DeleteTabGroupIDMetdata(const base::Uuid& sync_guid) override {
    cache_.erase(sync_guid);
  }

 private:
  std::map<base::Uuid, TabGroupIDMetadata> cache_;
};

}  // namespace

class TabGroupStoreTest : public testing::Test {
 public:
  TabGroupStoreTest() = default;
  ~TabGroupStoreTest() override = default;

  void SetUp() override {
    std::unique_ptr<TabGroupStoreDelegate> delegate =
        std::make_unique<FakeTabGroupStoreDelegate>();
    not_owned_delegate_ = delegate.get();
    store_ = std::make_unique<TabGroupStore>(std::move(delegate));
  }

  void TearDown() override {}

  void InitializeStoreAndWaitForCallback() {
    base::RunLoop run_loop;
    store_->Initialize(
        base::BindLambdaForTesting([&run_loop]() { run_loop.Quit(); }));
    run_loop.Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TabGroupStore> store_;
  raw_ptr<TabGroupStoreDelegate> not_owned_delegate_;
};

TEST_F(TabGroupStoreTest, InitialStateShouldBeEmpty) {
  auto entries = store_->GetAllTabGroupIDMetadata();
  EXPECT_EQ(0UL, entries.size());
}

TEST_F(TabGroupStoreTest, InitializeShouldReadDelegateState) {
  base::Uuid uuid = base::Uuid::GenerateRandomV4();
  TabGroupIDMetadata metadata(test::GenerateRandomTabGroupID());

  not_owned_delegate_->StoreTabGroupIDMetadata(uuid, metadata);

  InitializeStoreAndWaitForCallback();

  auto entries = store_->GetAllTabGroupIDMetadata();
  EXPECT_EQ(1UL, entries.size());
  EXPECT_EQ(1UL, entries.count(uuid));

  auto it = entries.find(uuid);
  EXPECT_FALSE(it == entries.end());
  EXPECT_EQ(metadata, it->second);

  std::optional<TabGroupIDMetadata> maybe_metadata =
      store_->GetTabGroupIDMetadata(uuid);
  EXPECT_TRUE(maybe_metadata.has_value());
  EXPECT_EQ(metadata, (*maybe_metadata));
}

TEST_F(TabGroupStoreTest, WriteAndReadMultipleEntries) {
  base::Uuid uuid1 = base::Uuid::GenerateRandomV4();
  TabGroupIDMetadata metadata1(test::GenerateRandomTabGroupID());
  base::Uuid uuid2 = base::Uuid::GenerateRandomV4();
  TabGroupIDMetadata metadata2(test::GenerateRandomTabGroupID());

  InitializeStoreAndWaitForCallback();

  // Add two metadatas.
  store_->StoreTabGroupIDMetadata(uuid1, metadata1);
  store_->StoreTabGroupIDMetadata(uuid2, metadata2);

  // Both metadatas should exist.
  auto entries = store_->GetAllTabGroupIDMetadata();
  EXPECT_EQ(2UL, entries.size());
  EXPECT_EQ(1UL, entries.count(uuid1));
  EXPECT_EQ(1UL, entries.count(uuid2));

  auto it1 = entries.find(uuid1);
  EXPECT_FALSE(it1 == entries.end());
  EXPECT_EQ(metadata1, it1->second);

  auto it2 = entries.find(uuid2);
  EXPECT_FALSE(it2 == entries.end());
  EXPECT_EQ(metadata2, it2->second);

  // Individual lookups should work.
  std::optional<TabGroupIDMetadata> maybe_metadata1 =
      store_->GetTabGroupIDMetadata(uuid1);
  EXPECT_TRUE(maybe_metadata1.has_value());
  EXPECT_EQ(metadata1, *maybe_metadata1);

  std::optional<TabGroupIDMetadata> maybe_metadata2 =
      store_->GetTabGroupIDMetadata(uuid2);
  EXPECT_TRUE(maybe_metadata2.has_value());
  EXPECT_EQ(metadata2, *maybe_metadata2);

  // Delete the first entry and validate state.
  store_->DeleteTabGroupIDMetadata(uuid1);
  entries = store_->GetAllTabGroupIDMetadata();
  EXPECT_EQ(1UL, entries.size());
  EXPECT_EQ(1UL, entries.count(uuid2));
  maybe_metadata1 = store_->GetTabGroupIDMetadata(uuid1);
  EXPECT_FALSE(maybe_metadata1.has_value());
  maybe_metadata2 = store_->GetTabGroupIDMetadata(uuid2);
  EXPECT_TRUE(maybe_metadata2.has_value());

  // Delete the second entry and validate state.
  store_->DeleteTabGroupIDMetadata(uuid2);
  entries = store_->GetAllTabGroupIDMetadata();
  EXPECT_EQ(0UL, entries.size());
  maybe_metadata1 = store_->GetTabGroupIDMetadata(uuid1);
  EXPECT_FALSE(maybe_metadata1.has_value());
  maybe_metadata2 = store_->GetTabGroupIDMetadata(uuid2);
  EXPECT_FALSE(maybe_metadata2.has_value());
}

}  // namespace tab_groups
