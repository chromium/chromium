// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_phishing_model.h"
#include "components/safe_browsing/content/browser/client_side_phishing_model_optimization_guide.h"

#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/safe_browsing/core/common/fbs/client_model_generated.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/client_model.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class ClientSidePhishingModelObserverTracker
    : public optimization_guide::TestOptimizationGuideModelProvider {
 public:
  void AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const absl::optional<optimization_guide::proto::Any>& model_metadata,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
      EXPECT_FALSE(model_observer_);
      model_observer_ = observer;
    }
  }

  void RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OptimizationTarget optimization_target,
      optimization_guide::OptimizationTargetModelObserver* observer) override {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
      EXPECT_EQ(observer, model_observer_);
      model_observer_ = nullptr;
    }
  }

  // Notifies the model validation observer about the model file update.
  void NotifyModelFileUpdate(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const base::FilePath& model_file_path,
      const base::flat_set<base::FilePath>& additional_files_path) {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
      auto model_metadata = optimization_guide::TestModelInfoBuilder()
                                .SetModelFilePath(model_file_path)
                                .SetAdditionalFiles(additional_files_path)
                                .Build();
      model_observer_->OnModelUpdated(optimization_target, *model_metadata);
    } else if (optimization_target ==
               optimization_guide::proto::
                   OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING_IMAGE_EMBEDDER) {
      auto model_metadata = optimization_guide::TestModelInfoBuilder()
                                .SetModelFilePath(model_file_path)
                                .Build();
      model_observer_->OnModelUpdated(optimization_target, *model_metadata);
    }
  }

 private:
  // The observer that is registered to receive model validation optimzation
  // target events.
  raw_ptr<optimization_guide::OptimizationTargetModelObserver> model_observer_;
};

class ClientSidePhishingModelTest
    : public content::RenderViewHostTestHarness,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  ClientSidePhishingModelTest() {
    std::vector<base::test::FeatureRef> enabled_features = {};
    if (ShouldEnableCacao()) {
      enabled_features.push_back(kClientSideDetectionModelOptimizationGuide);
    }

    if (ShouldEnableImageEmbedder()) {
      enabled_features.push_back(kClientSideDetectionModelImageEmbedder);
    }

    feature_list_.InitWithFeatures(enabled_features, {});
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    if (base::FeatureList::IsEnabled(
            kClientSideDetectionModelOptimizationGuide)) {
      model_observer_tracker_ =
          std::make_unique<ClientSidePhishingModelObserverTracker>();
      scoped_refptr<base::SequencedTaskRunner> background_task_runner =
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
      client_side_phishing_model_ =
          std::make_unique<ClientSidePhishingModelOptimizationGuide>(
              model_observer_tracker_.get(), background_task_runner);
    }
  }

  void TearDown() override {
    content::RenderViewHostTestHarness::TearDown();
    client_side_phishing_model_.reset();
    model_observer_tracker_.reset();
  }

  void ValidateModel(
      const base::FilePath& model_file_path,
      const base::flat_set<base::FilePath>& additional_file_path) {
    model_observer_tracker_->NotifyModelFileUpdate(
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING,
        model_file_path, additional_file_path);
    task_environment()->RunUntilIdle();
  }

  void ValidateImageEmbeddingModel(
      const base::FilePath& image_embedding_model_file_path) {
    model_observer_tracker_->NotifyModelFileUpdate(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING_IMAGE_EMBEDDER,
        image_embedding_model_file_path, {});
    task_environment()->RunUntilIdle();
  }

  ClientSidePhishingModelOptimizationGuide* service() {
    return client_side_phishing_model_.get();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  bool ShouldEnableCacao() { return get<0>(GetParam()); }

  bool ShouldEnableImageEmbedder() { return get<1>(GetParam()); }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;

  std::unique_ptr<ClientSidePhishingModelObserverTracker>
      model_observer_tracker_;
  std::unique_ptr<ClientSidePhishingModelOptimizationGuide>
      client_side_phishing_model_;
};

namespace {

std::string CreateFlatBufferString() {
  flatbuffers::FlatBufferBuilder builder(1024);
  flat::ClientSideModelBuilder csd_model_builder(builder);
  builder.Finish(csd_model_builder.Finish());
  return std::string(reinterpret_cast<char*>(builder.GetBufferPointer()),
                     builder.GetSize());
}

void GetFlatBufferStringFromMappedMemory(
    base::ReadOnlySharedMemoryRegion region,
    std::string* output) {
  ASSERT_TRUE(region.IsValid());
  base::ReadOnlySharedMemoryMapping mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());
  *output = std::string(reinterpret_cast<const char*>(mapping.memory()),
                        mapping.size());
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(All,
                         ClientSidePhishingModelTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(ClientSidePhishingModelTest, ValidModel) {
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    return;
  }
  base::FilePath model_file_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &model_file_path);
  model_file_path = model_file_path.AppendASCII("components")
                        .AppendASCII("test")
                        .AppendASCII("data")
                        .AppendASCII("safe_browsing")
                        .AppendASCII("client_model.pb");

  base::FilePath additional_files_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &additional_files_path);
  additional_files_path = additional_files_path.AppendASCII("components")
                              .AppendASCII("test")
                              .AppendASCII("data")
                              .AppendASCII("safe_browsing");

#if BUILDFLAG(IS_ANDROID)
  additional_files_path =
      additional_files_path.AppendASCII("visual_model_android.tflite");
#else
  additional_files_path =
      additional_files_path.AppendASCII("visual_model_desktop.tflite");
#endif
  service()->SetModelTypeForTesting(CSDModelTypeOptimizationGuide::kFlatbuffer);
  ValidateModel(model_file_path, {additional_files_path});

  histogram_tester().ExpectUniqueSample(
      "SBClientPhishing.ModelDynamicUpdateSuccess", true, 1);
  EXPECT_TRUE(service()->IsEnabled());
  if (base::FeatureList::IsEnabled(kClientSideDetectionModelImageEmbedder)) {
    base::FilePath image_embedding_model_file_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT,
                           &image_embedding_model_file_path);
    image_embedding_model_file_path =
        image_embedding_model_file_path.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("safe_browsing")
            .AppendASCII("image_embedding.tflite");
    ValidateImageEmbeddingModel(image_embedding_model_file_path);
    histogram_tester().ExpectUniqueSample(
        "SBClientPhishing.ModelDynamicUpdateSuccess.ImageEmbedding", true, 1);
  }
}

TEST_P(ClientSidePhishingModelTest, InvalidModelDueToInvalidPath) {
  if (!base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    return;
  }
  base::ScopedTempDir model_dir;
  EXPECT_TRUE(model_dir.CreateUniqueTempDir());
  base::FilePath model_file_path =
      model_dir.GetPath().AppendASCII("non_existent_client_model.pb");
  base::WriteFile(model_file_path, "INVALID MODEL DATA");

  base::FilePath additional_files_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &additional_files_path);
  additional_files_path = additional_files_path.AppendASCII("components")
                              .AppendASCII("test")
                              .AppendASCII("data")
                              .AppendASCII("safe_browsing");

#if BUILDFLAG(IS_ANDROID)
  additional_files_path =
      additional_files_path.AppendASCII("visual_model_android.tflite");
#else
  additional_files_path =
      additional_files_path.AppendASCII("visual_model_desktop.tflite");
#endif

  ValidateModel(model_file_path, {additional_files_path});

  histogram_tester().ExpectUniqueSample(
      "SBClientPhishing.ModelDynamicUpdateSuccess", false, 1);
  EXPECT_FALSE(ClientSidePhishingModel::GetInstance()->IsEnabled());
}

TEST_P(ClientSidePhishingModelTest, NotifiesOnUpdate) {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStrForTesting("");
    service()->SetVisualTfLiteModelForTesting(base::File());
    service()->ClearMappedRegionForTesting();
    service()->SetModelTypeForTesting(CSDModelTypeOptimizationGuide::kNone);
  } else {
    ClientSidePhishingModel::GetInstance()->SetModelStrForTesting("");
    ClientSidePhishingModel::GetInstance()->SetVisualTfLiteModelForTesting(
        base::File());
    ClientSidePhishingModel::GetInstance()->ClearMappedRegionForTesting();
    ClientSidePhishingModel::GetInstance()->SetModelTypeForTesting(
        CSDModelType::kNone);
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kClientSideDetectionModelIsFlatBuffer);

  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      base::FeatureList::IsEnabled(kClientSideDetectionModelOptimizationGuide)
          ? service()->RegisterCallback(base::BindRepeating(
                [](base::RepeatingClosure quit_closure, bool* called) {
                  *called = true;
                  std::move(quit_closure).Run();
                },
                run_loop.QuitClosure(), &called))
          : ClientSidePhishingModel::GetInstance()->RegisterCallback(
                base::BindRepeating(
                    [](base::RepeatingClosure quit_closure, bool* called) {
                      *called = true;
                      std::move(quit_closure).Run();
                    },
                    run_loop.QuitClosure(), &called));

  ClientSideModel model;
  model.set_max_words_per_term(0);  // Required field.
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStringForTesting(model.SerializeAsString(),
                                        base::File());
  } else {
    ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
        model.SerializeAsString(), base::File());
  }

  run_loop.Run();

  EXPECT_TRUE(called);
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    EXPECT_EQ(model.SerializeAsString(), service()->GetModelStr());
    EXPECT_TRUE(service()->IsEnabled());
  } else {
    EXPECT_EQ(model.SerializeAsString(),
              ClientSidePhishingModel::GetInstance()->GetModelStr());
    EXPECT_TRUE(ClientSidePhishingModel::GetInstance()->IsEnabled());
  }
}

TEST_P(ClientSidePhishingModelTest, RejectsInvalidProto) {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStrForTesting("");
    service()->SetVisualTfLiteModelForTesting(base::File());
    service()->ClearMappedRegionForTesting();
    service()->SetModelTypeForTesting(CSDModelTypeOptimizationGuide::kNone);
    service()->SetModelStringForTesting("bad proto", base::File());
    EXPECT_FALSE(service()->IsEnabled());
  } else {
    ClientSidePhishingModel::GetInstance()->SetModelStrForTesting("");
    ClientSidePhishingModel::GetInstance()->SetVisualTfLiteModelForTesting(
        base::File());
    ClientSidePhishingModel::GetInstance()->ClearMappedRegionForTesting();
    ClientSidePhishingModel::GetInstance()->SetModelTypeForTesting(
        CSDModelType::kNone);
    ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
        "bad proto", base::File());
    EXPECT_FALSE(ClientSidePhishingModel::GetInstance()->IsEnabled());
  }
}

TEST_P(ClientSidePhishingModelTest, RejectsInvalidFlatbuffer) {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStrForTesting("");
    service()->SetVisualTfLiteModelForTesting(base::File());
    service()->ClearMappedRegionForTesting();
    service()->SetModelTypeForTesting(CSDModelTypeOptimizationGuide::kNone);
  } else {
    ClientSidePhishingModel::GetInstance()->SetModelStrForTesting("");
    ClientSidePhishingModel::GetInstance()->SetVisualTfLiteModelForTesting(
        base::File());
    ClientSidePhishingModel::GetInstance()->ClearMappedRegionForTesting();
    ClientSidePhishingModel::GetInstance()->SetModelTypeForTesting(
        CSDModelType::kNone);
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kClientSideDetectionModelIsFlatBuffer},
      /*disabled_features=*/{});
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStringForTesting("bad flatbuffer", base::File());
    EXPECT_FALSE(service()->IsEnabled());
  } else {
    ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
        "bad flatbuffer", base::File());
    EXPECT_FALSE(ClientSidePhishingModel::GetInstance()->IsEnabled());
  }
}

TEST_P(ClientSidePhishingModelTest, NotifiesForFile) {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStrForTesting("");
    service()->SetVisualTfLiteModelForTesting(base::File());
    service()->ClearMappedRegionForTesting();
    service()->SetModelTypeForTesting(CSDModelTypeOptimizationGuide::kNone);
  } else {
    ClientSidePhishingModel::GetInstance()->SetModelStrForTesting("");
    ClientSidePhishingModel::GetInstance()->SetVisualTfLiteModelForTesting(
        base::File());
    ClientSidePhishingModel::GetInstance()->ClearMappedRegionForTesting();
    ClientSidePhishingModel::GetInstance()->SetModelTypeForTesting(
        CSDModelType::kNone);
  }

  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      base::FeatureList::IsEnabled(kClientSideDetectionModelOptimizationGuide)
          ? service()->RegisterCallback(base::BindRepeating(
                [](base::RepeatingClosure quit_closure, bool* called) {
                  *called = true;
                  std::move(quit_closure).Run();
                },
                run_loop.QuitClosure(), &called))
          : ClientSidePhishingModel::GetInstance()->RegisterCallback(
                base::BindRepeating(
                    [](base::RepeatingClosure quit_closure, bool* called) {
                      *called = true;
                      std::move(quit_closure).Run();
                    },
                    run_loop.QuitClosure(), &called));

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("visual_model.tflite");
  base::File file(file_path, base::File::FLAG_OPEN_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  const std::string file_contents = "visual model file";
  file.WriteAtCurrentPos(file_contents.data(), file_contents.size());

  const std::string model_str = CreateFlatBufferString();
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStringForTesting(model_str, std::move(file));
  } else {
    ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
        model_str, std::move(file));
  }

  run_loop.Run();

  EXPECT_TRUE(called);
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    EXPECT_TRUE(service()->IsEnabled());
  } else {
    EXPECT_TRUE(ClientSidePhishingModel::GetInstance()->IsEnabled());
  }
}

TEST_P(ClientSidePhishingModelTest, DoesNotNotifyOnBadInitialUpdate) {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStrForTesting("");
    service()->SetVisualTfLiteModelForTesting(base::File());
    service()->ClearMappedRegionForTesting();
    service()->SetModelTypeForTesting(CSDModelTypeOptimizationGuide::kNone);
  } else {
    ClientSidePhishingModel::GetInstance()->SetModelStrForTesting("");
    ClientSidePhishingModel::GetInstance()->SetVisualTfLiteModelForTesting(
        base::File());
    ClientSidePhishingModel::GetInstance()->ClearMappedRegionForTesting();
    ClientSidePhishingModel::GetInstance()->SetModelTypeForTesting(
        CSDModelType::kNone);
  }

  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      base::FeatureList::IsEnabled(kClientSideDetectionModelOptimizationGuide)
          ? service()->RegisterCallback(base::BindRepeating(
                [](base::RepeatingClosure quit_closure, bool* called) {
                  *called = true;
                  std::move(quit_closure).Run();
                },
                run_loop.QuitClosure(), &called))
          : ClientSidePhishingModel::GetInstance()->RegisterCallback(
                base::BindRepeating(
                    [](base::RepeatingClosure quit_closure, bool* called) {
                      *called = true;
                      std::move(quit_closure).Run();
                    },
                    run_loop.QuitClosure(), &called));

  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStringForTesting("", base::File());
  } else {
    ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
        "", base::File());
  }

  run_loop.RunUntilIdle();

  EXPECT_FALSE(called);
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    EXPECT_FALSE(service()->IsEnabled());
  } else {
    EXPECT_FALSE(ClientSidePhishingModel::GetInstance()->IsEnabled());
  }
}

TEST_P(ClientSidePhishingModelTest, DoesNotNotifyOnBadFollowingUpdate) {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStrForTesting("");
    service()->SetVisualTfLiteModelForTesting(base::File());
    service()->ClearMappedRegionForTesting();
    service()->SetModelTypeForTesting(CSDModelTypeOptimizationGuide::kNone);
  } else {
    ClientSidePhishingModel::GetInstance()->SetModelStrForTesting("");
    ClientSidePhishingModel::GetInstance()->SetVisualTfLiteModelForTesting(
        base::File());
    ClientSidePhishingModel::GetInstance()->ClearMappedRegionForTesting();
    ClientSidePhishingModel::GetInstance()->SetModelTypeForTesting(
        CSDModelType::kNone);
  }

  base::RunLoop run_loop;

  // Perform a valid update.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("visual_model.tflite");
  base::File file(file_path, base::File::FLAG_OPEN_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  const std::string file_contents = "visual model file";
  file.WriteAtCurrentPos(file_contents.data(), file_contents.size());

  const std::string model_str = CreateFlatBufferString();
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStringForTesting(model_str, std::move(file));
  } else {
    ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
        model_str, std::move(file));
  }

  run_loop.RunUntilIdle();

  // Perform an invalid update.
  bool called = false;
  base::CallbackListSubscription subscription =
      base::FeatureList::IsEnabled(kClientSideDetectionModelOptimizationGuide)
          ? service()->RegisterCallback(base::BindRepeating(
                [](base::RepeatingClosure quit_closure, bool* called) {
                  *called = true;
                  std::move(quit_closure).Run();
                },
                run_loop.QuitClosure(), &called))
          : ClientSidePhishingModel::GetInstance()->RegisterCallback(
                base::BindRepeating(
                    [](base::RepeatingClosure quit_closure, bool* called) {
                      *called = true;
                      std::move(quit_closure).Run();
                    },
                    run_loop.QuitClosure(), &called));

  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStringForTesting("", base::File());
  } else {
    ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
        "", base::File());
  }

  run_loop.RunUntilIdle();

  EXPECT_FALSE(called);
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    EXPECT_TRUE(service()->IsEnabled());
  } else {
    EXPECT_TRUE(ClientSidePhishingModel::GetInstance()->IsEnabled());
  }
}

TEST_P(ClientSidePhishingModelTest, CanOverrideFlatBufferWithFlag) {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStrForTesting("");
    service()->SetVisualTfLiteModelForTesting(base::File());
    service()->ClearMappedRegionForTesting();
    service()->SetModelTypeForTesting(CSDModelTypeOptimizationGuide::kNone);
  } else {
    ClientSidePhishingModel::GetInstance()->SetModelStrForTesting("");
    ClientSidePhishingModel::GetInstance()->SetVisualTfLiteModelForTesting(
        base::File());
    ClientSidePhishingModel::GetInstance()->ClearMappedRegionForTesting();
    ClientSidePhishingModel::GetInstance()->SetModelTypeForTesting(
        CSDModelType::kNone);
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kClientSideDetectionModelIsFlatBuffer},
      /*disabled_features=*/{});
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path = temp_dir.GetPath();
  base::File file(file_path.AppendASCII("client_model.pb"),
                  base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                      base::File::FLAG_WRITE);

  const std::string file_contents = CreateFlatBufferString();
  file.WriteAtCurrentPos(file_contents.data(), file_contents.size());

  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "csd-model-override-path", file_path);

  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      base::FeatureList::IsEnabled(kClientSideDetectionModelOptimizationGuide)
          ? service()->RegisterCallback(base::BindRepeating(
                [](base::RepeatingClosure quit_closure, bool* called) {
                  *called = true;
                  std::move(quit_closure).Run();
                },
                run_loop.QuitClosure(), &called))
          : ClientSidePhishingModel::GetInstance()->RegisterCallback(
                base::BindRepeating(
                    [](base::RepeatingClosure quit_closure, bool* called) {
                      *called = true;
                      std::move(quit_closure).Run();
                    },
                    run_loop.QuitClosure(), &called));

  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->MaybeOverrideModel();
  } else {
    ClientSidePhishingModel::GetInstance()->MaybeOverrideModel();
  }

  run_loop.Run();

  std::string model_str_from_shared_mem;
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    ASSERT_NO_FATAL_FAILURE(GetFlatBufferStringFromMappedMemory(
        service()->GetModelSharedMemoryRegion(), &model_str_from_shared_mem));
    EXPECT_EQ(model_str_from_shared_mem, file_contents);
    EXPECT_EQ(service()->GetModelType(),
              CSDModelTypeOptimizationGuide::kFlatbuffer);
  } else {
    ASSERT_NO_FATAL_FAILURE(GetFlatBufferStringFromMappedMemory(
        ClientSidePhishingModel::GetInstance()->GetModelSharedMemoryRegion(),
        &model_str_from_shared_mem));
    EXPECT_EQ(model_str_from_shared_mem, file_contents);
    EXPECT_EQ(ClientSidePhishingModel::GetInstance()->GetModelType(),
              CSDModelType::kFlatbuffer);
  }
  EXPECT_TRUE(called);
}

TEST_P(ClientSidePhishingModelTest, AcceptsValidFlatbufferIfFeatureEnabled) {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStrForTesting("");
    service()->SetVisualTfLiteModelForTesting(base::File());
    service()->ClearMappedRegionForTesting();
    service()->SetModelTypeForTesting(CSDModelTypeOptimizationGuide::kNone);
  } else {
    ClientSidePhishingModel::GetInstance()->SetModelStrForTesting("");
    ClientSidePhishingModel::GetInstance()->SetVisualTfLiteModelForTesting(
        base::File());
    ClientSidePhishingModel::GetInstance()->ClearMappedRegionForTesting();
    ClientSidePhishingModel::GetInstance()->SetModelTypeForTesting(
        CSDModelType::kNone);
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kClientSideDetectionModelIsFlatBuffer},
      /*disabled_features=*/{});
  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      base::FeatureList::IsEnabled(kClientSideDetectionModelOptimizationGuide)
          ? service()->RegisterCallback(base::BindRepeating(
                [](base::RepeatingClosure quit_closure, bool* called) {
                  *called = true;
                  std::move(quit_closure).Run();
                },
                run_loop.QuitClosure(), &called))
          : ClientSidePhishingModel::GetInstance()->RegisterCallback(
                base::BindRepeating(
                    [](base::RepeatingClosure quit_closure, bool* called) {
                      *called = true;
                      std::move(quit_closure).Run();
                    },
                    run_loop.QuitClosure(), &called));

  const std::string model_str = CreateFlatBufferString();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("visual_model.tflite");
  base::File file(file_path, base::File::FLAG_OPEN_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  const std::string file_contents = "visual model file";
  file.WriteAtCurrentPos(file_contents.data(), file_contents.size());
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStringForTesting(model_str, std::move(file));
  } else {
    ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
        model_str, std::move(file));
  }
  run_loop.Run();

  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    EXPECT_TRUE(service()->IsEnabled());
  } else {
    EXPECT_TRUE(ClientSidePhishingModel::GetInstance()->IsEnabled());
  }
  std::string model_str_from_shared_mem;
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    ASSERT_NO_FATAL_FAILURE(GetFlatBufferStringFromMappedMemory(
        service()->GetModelSharedMemoryRegion(), &model_str_from_shared_mem));
    EXPECT_EQ(model_str, model_str_from_shared_mem);
    EXPECT_EQ(service()->GetModelType(),
              CSDModelTypeOptimizationGuide::kFlatbuffer);
  } else {
    ASSERT_NO_FATAL_FAILURE(GetFlatBufferStringFromMappedMemory(
        ClientSidePhishingModel::GetInstance()->GetModelSharedMemoryRegion(),
        &model_str_from_shared_mem));
    EXPECT_EQ(model_str, model_str_from_shared_mem);
    EXPECT_EQ(ClientSidePhishingModel::GetInstance()->GetModelType(),
              CSDModelType::kFlatbuffer);
  }
  EXPECT_TRUE(called);
}

TEST_P(ClientSidePhishingModelTest, FlatbufferonFollowingUpdate) {
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStrForTesting("");
    service()->SetVisualTfLiteModelForTesting(base::File());
    service()->ClearMappedRegionForTesting();
    service()->SetModelTypeForTesting(CSDModelTypeOptimizationGuide::kNone);
  } else {
    ClientSidePhishingModel::GetInstance()->SetModelStrForTesting("");
    ClientSidePhishingModel::GetInstance()->SetVisualTfLiteModelForTesting(
        base::File());
    ClientSidePhishingModel::GetInstance()->ClearMappedRegionForTesting();
    ClientSidePhishingModel::GetInstance()->SetModelTypeForTesting(
        CSDModelType::kNone);
  }
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kClientSideDetectionModelIsFlatBuffer},
      /*disabled_features=*/{});
  base::RunLoop run_loop;

  const std::string model_str1 = CreateFlatBufferString();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("visual_model.tflite");
  base::File file(file_path, base::File::FLAG_OPEN_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  const std::string file_contents = "visual model file";
  file.WriteAtCurrentPos(file_contents.data(), file_contents.size());
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStringForTesting(model_str1, std::move(file));
  } else {
    ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
        model_str1, std::move(file));
  }

  run_loop.RunUntilIdle();
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    EXPECT_TRUE(service()->IsEnabled());
  } else {
    EXPECT_TRUE(ClientSidePhishingModel::GetInstance()->IsEnabled());
  }
  std::string model_str_from_shared_mem1;
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    ASSERT_NO_FATAL_FAILURE(GetFlatBufferStringFromMappedMemory(
        service()->GetModelSharedMemoryRegion(), &model_str_from_shared_mem1));
    EXPECT_EQ(model_str1, model_str_from_shared_mem1);
    EXPECT_EQ(service()->GetModelType(),
              CSDModelTypeOptimizationGuide::kFlatbuffer);
  } else {
    ASSERT_NO_FATAL_FAILURE(GetFlatBufferStringFromMappedMemory(
        ClientSidePhishingModel::GetInstance()->GetModelSharedMemoryRegion(),
        &model_str_from_shared_mem1));
    EXPECT_EQ(model_str1, model_str_from_shared_mem1);
    EXPECT_EQ(ClientSidePhishingModel::GetInstance()->GetModelType(),
              CSDModelType::kFlatbuffer);
  }

  // Should be able to write to memory with WritableSharedMemoryMapping field.
  void* memory_addr =
      base::FeatureList::IsEnabled(kClientSideDetectionModelOptimizationGuide)
          ? service()->GetFlatBufferMemoryAddressForTesting()
          : ClientSidePhishingModel::GetInstance()
                ->GetFlatBufferMemoryAddressForTesting();

  EXPECT_EQ(memset(memory_addr, 'G', 1), memory_addr);

  bool called = false;
  base::CallbackListSubscription subscription =
      base::FeatureList::IsEnabled(kClientSideDetectionModelOptimizationGuide)
          ? service()->RegisterCallback(base::BindRepeating(
                [](base::RepeatingClosure quit_closure, bool* called) {
                  *called = true;
                  std::move(quit_closure).Run();
                },
                run_loop.QuitClosure(), &called))
          : ClientSidePhishingModel::GetInstance()->RegisterCallback(
                base::BindRepeating(
                    [](base::RepeatingClosure quit_closure, bool* called) {
                      *called = true;
                      std::move(quit_closure).Run();
                    },
                    run_loop.QuitClosure(), &called));

  const std::string model_str2 = CreateFlatBufferString();
  base::ScopedTempDir temp_dir2;
  ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());
  const base::FilePath file_path2 =
      temp_dir2.GetPath().AppendASCII("visual_model.tflite");
  base::File file2(file_path2, base::File::FLAG_OPEN_ALWAYS |
                                   base::File::FLAG_READ |
                                   base::File::FLAG_WRITE);
  const std::string file_contents2 = "visual model file";
  file2.WriteAtCurrentPos(file_contents2.data(), file_contents2.size());
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    service()->SetModelStringForTesting(model_str2, std::move(file2));
  } else {
    ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
        model_str2, std::move(file2));
  }

  run_loop.RunUntilIdle();
  EXPECT_TRUE(called);
  if (base::FeatureList::IsEnabled(
          kClientSideDetectionModelOptimizationGuide)) {
    EXPECT_TRUE(service()->IsEnabled());
    std::string model_str_from_shared_mem2;
    ASSERT_NO_FATAL_FAILURE(GetFlatBufferStringFromMappedMemory(
        service()->GetModelSharedMemoryRegion(), &model_str_from_shared_mem2));
    EXPECT_EQ(model_str2, model_str_from_shared_mem2);
    EXPECT_EQ(service()->GetModelType(),
              CSDModelTypeOptimizationGuide::kFlatbuffer);
  } else {
    EXPECT_TRUE(ClientSidePhishingModel::GetInstance()->IsEnabled());
    std::string model_str_from_shared_mem2;
    ASSERT_NO_FATAL_FAILURE(GetFlatBufferStringFromMappedMemory(
        ClientSidePhishingModel::GetInstance()->GetModelSharedMemoryRegion(),
        &model_str_from_shared_mem2));
    EXPECT_EQ(model_str2, model_str_from_shared_mem2);
    EXPECT_EQ(ClientSidePhishingModel::GetInstance()->GetModelType(),
              CSDModelType::kFlatbuffer);
  }

  // Mapping should be undone automatically, even with a region copy lying
  // around.
  // Can remove this if flaky.
  // Windows ASAN flake: crbug.com/1234652
#if !(BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER))
  BASE_EXPECT_DEATH(memset(memory_addr, 'G', 1), "");
#endif
}

}  // namespace safe_browsing
