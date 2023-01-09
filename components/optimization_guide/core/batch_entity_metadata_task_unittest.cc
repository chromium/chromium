// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/batch_entity_metadata_task.h"

#include "base/test/scoped_feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/entity_metadata_provider.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

class TestEntityMetadataProvider : public EntityMetadataProvider {
 public:
  explicit TestEntityMetadataProvider(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
      : main_thread_task_runner_(main_thread_task_runner) {}
  ~TestEntityMetadataProvider() override = default;

  // EntityMetadataProvider:
  void GetMetadataForEntityId(
      const std::string& entity_id,
      EntityMetadataRetrievedCallback callback) override {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const std::string& entity_id,
               EntityMetadataRetrievedCallback callback) {
              EntityMetadata metadata;
              metadata.human_readable_name = entity_id;
              std::move(callback).Run(entity_id == "nometadata"
                                          ? absl::nullopt
                                          : absl::make_optional(metadata));
            },
            entity_id, std::move(callback)));
  }
  void GetMetadataForEntityIds(
      const base::flat_set<std::string>& entity_ids,
      BatchEntityMetadataRetrievedCallback callback) override {
    main_thread_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](const base::flat_set<std::string>& entity_ids,
               BatchEntityMetadataRetrievedCallback callback) {
              base::flat_map<std::string, EntityMetadata> entity_metadata_map;
              for (const auto& entity_id : entity_ids) {
                if (entity_id == "nometadata") {
                  continue;
                }
                EntityMetadata metadata;
                metadata.human_readable_name = entity_id;
                entity_metadata_map[entity_id] = metadata;
              }
              std::move(callback).Run(entity_metadata_map);
            },
            entity_ids, std::move(callback)));
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
};

class BatchEntityMetadataTaskTest : public testing::Test,
                                    public testing::WithParamInterface<bool> {
 public:
  BatchEntityMetadataTaskTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kPageEntitiesModelBatchEntityMetadataSimplification);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kPageEntitiesModelBatchEntityMetadataSimplification);
    }
  }
  void SetUp() override {
    entity_metadata_provider_ = std::make_unique<TestEntityMetadataProvider>(
        task_environment_.GetMainThreadTaskRunner());
  }

  void TearDown() override { entity_metadata_provider_.reset(); }

  base::flat_map<std::string, EntityMetadata> ExecuteBatchEntityMetadataTask(
      const std::vector<std::string>& entity_ids) {
    auto task = std::make_unique<BatchEntityMetadataTask>(
        entity_metadata_provider_.get(), entity_ids);

    base::flat_map<std::string, EntityMetadata> entity_metadata_map;

    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    task->Execute(base::BindOnce(
        [](base::RunLoop* run_loop,
           base::flat_map<std::string, EntityMetadata>* out_entity_metadata_map,
           const base::flat_map<std::string, EntityMetadata>&
               entity_metadata_map) {
          *out_entity_metadata_map = entity_metadata_map;
          run_loop->Quit();
        },
        run_loop.get(), &entity_metadata_map));
    run_loop->Run();

    return entity_metadata_map;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<TestEntityMetadataProvider> entity_metadata_provider_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BatchEntityMetadataTaskTest,
    /*ShouldUseBatchEntityMetadataSimplification=*/testing::Bool());

TEST_P(BatchEntityMetadataTaskTest, Execute) {
  base::flat_map<std::string, EntityMetadata> entity_metadata_map =
      ExecuteBatchEntityMetadataTask({
          "nometadata",
          "someentity",
      });

  EXPECT_EQ(entity_metadata_map.size(), 1u);
  auto it = entity_metadata_map.find("someentity");
  ASSERT_NE(it, entity_metadata_map.end());
  EXPECT_EQ(it->second.human_readable_name, "someentity");
}

}  // namespace
}  // namespace optimization_guide
