// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/local_page_entities_metadata_provider.h"

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class LocalPageEntitiesMetadataProviderTest : public testing::Test {
 public:
  LocalPageEntitiesMetadataProviderTest() = default;
  ~LocalPageEntitiesMetadataProviderTest() override = default;

  void SetUp() override {
    auto db = std::make_unique<
        leveldb_proto::test::FakeDB<proto::EntityMetadataStorage>>(&db_store_);
    db_ = db.get();

    provider_ = std::make_unique<LocalPageEntitiesMetadataProvider>();
    provider_->InitializeForTesting(
        std::move(db), task_environment_.GetMainThreadTaskRunner());
  }

  LocalPageEntitiesMetadataProvider* provider() { return provider_.get(); }

  leveldb_proto::test::FakeDB<proto::EntityMetadataStorage>* db() {
    return db_;
  }

  std::map<std::string, proto::EntityMetadataStorage>* store() {
    return &db_store_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<LocalPageEntitiesMetadataProvider> provider_;
  raw_ptr<leveldb_proto::test::FakeDB<proto::EntityMetadataStorage>> db_;
  std::map<std::string, proto::EntityMetadataStorage> db_store_;
};

TEST_F(LocalPageEntitiesMetadataProviderTest, NonInitReturnsNullOpt) {
  LocalPageEntitiesMetadataProvider provider;

  absl::optional<EntityMetadata> md;
  bool callback_ran = false;
  provider.GetMetadataForEntityId(
      "entity_id",
      base::BindOnce(
          [](bool* callback_ran_flag, absl::optional<EntityMetadata>* md_out,
             const absl::optional<EntityMetadata>& md_in) {
            *callback_ran_flag = true;
            *md_out = md_in;
          },
          &callback_ran, &md));

  ASSERT_TRUE(callback_ran);
  EXPECT_EQ(absl::nullopt, md);
}

TEST_F(LocalPageEntitiesMetadataProviderTest, EmptyStoreReturnsNullOpt) {
  absl::optional<EntityMetadata> md;
  bool callback_ran = false;
  provider()->GetMetadataForEntityId(
      "entity_id",
      base::BindOnce(
          [](bool* callback_ran_flag, absl::optional<EntityMetadata>* md_out,
             const absl::optional<EntityMetadata>& md_in) {
            *callback_ran_flag = true;
            *md_out = md_in;
          },
          &callback_ran, &md));

  db()->GetCallback(/*success=*/true);

  ASSERT_TRUE(callback_ran);
  EXPECT_EQ(absl::nullopt, md);
}

TEST_F(LocalPageEntitiesMetadataProviderTest, PopulatedSuccess) {
  proto::EntityMetadataStorage stored_proto;
  stored_proto.set_entity_name("chip");
  store()->emplace("chocolate", stored_proto);

  EntityMetadata want_md;
  want_md.entity_id = "chocolate";
  want_md.human_readable_name = "chip";

  absl::optional<EntityMetadata> md;
  bool callback_ran = false;
  provider()->GetMetadataForEntityId(
      "chocolate",
      base::BindOnce(
          [](bool* callback_ran_flag, absl::optional<EntityMetadata>* md_out,
             const absl::optional<EntityMetadata>& md_in) {
            *callback_ran_flag = true;
            *md_out = md_in;
          },
          &callback_ran, &md));

  db()->GetCallback(/*success=*/true);

  ASSERT_TRUE(callback_ran);
  EXPECT_EQ(absl::make_optional(want_md), md);
}

TEST_F(LocalPageEntitiesMetadataProviderTest, PopulatedFailure) {
  proto::EntityMetadataStorage stored_proto;
  stored_proto.set_entity_name("chip");
  store()->emplace("chocolate", stored_proto);

  absl::optional<EntityMetadata> md;
  bool callback_ran = false;
  provider()->GetMetadataForEntityId(
      "chocolate",
      base::BindOnce(
          [](bool* callback_ran_flag, absl::optional<EntityMetadata>* md_out,
             const absl::optional<EntityMetadata>& md_in) {
            *callback_ran_flag = true;
            *md_out = md_in;
          },
          &callback_ran, &md));

  db()->GetCallback(/*success=*/false);

  ASSERT_TRUE(callback_ran);
  EXPECT_EQ(absl::nullopt, md);
}

}  // namespace optimization_guide