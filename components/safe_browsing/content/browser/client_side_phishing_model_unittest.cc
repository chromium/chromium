// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_phishing_model.h"

#include <string>
#include <utility>

#include "base/containers/span.h"
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
#include "components/optimization_guide/proto/client_side_phishing_model_metadata.pb.h"
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
      const std::optional<optimization_guide::proto::Any>& model_metadata,
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

  optimization_guide::proto::Any WrapMetadata(
      std::optional<optimization_guide::proto::ClientSidePhishingModelMetadata>
          metadata) {
    std::string serialized_metadata;
    metadata->SerializeToString(&serialized_metadata);
    optimization_guide::proto::Any any;
    any.set_value(serialized_metadata);
    any.set_type_url(
        "type.googleapis.com/"
        "optimization_guide.proto.ClientSidePhishingModelMetadata");
    return any;
  }

  // Notifies the model validation observer about the model file update.
  void NotifyModelFileUpdate(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const base::FilePath& model_file_path,
      const base::flat_set<base::FilePath>& additional_files_path) {
    if (optimization_target ==
        optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING) {
      optimization_guide::proto::ClientSidePhishingModelMetadata
          trigger_model_metadata;
      trigger_model_metadata.set_image_embedding_model_version(1);
      auto model_metadata =
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path)
              .SetAdditionalFiles(additional_files_path)
              .SetModelMetadata(WrapMetadata(trigger_model_metadata))
              .Build();
      model_observer_->OnModelUpdated(optimization_target, *model_metadata);
    } else if (optimization_target ==
               optimization_guide::proto::
                   OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING_IMAGE_EMBEDDER) {
      optimization_guide::proto::ClientSidePhishingModelMetadata
          image_embedding_model_metadata;
      image_embedding_model_metadata.set_image_embedding_model_version(1);
      auto model_metadata =
          optimization_guide::TestModelInfoBuilder()
              .SetModelFilePath(model_file_path)
              .SetModelMetadata(WrapMetadata(image_embedding_model_metadata))
              .Build();
      model_observer_->OnModelUpdated(optimization_target, *model_metadata);
    }
  }

  void SendEmptyModelInfoUpdate(
      optimization_guide::proto::OptimizationTarget optimization_target) {
    model_observer_->OnModelUpdated(optimization_target, std::nullopt);
  }

 private:
  // The observer that is registered to receive model validation optimzation
  // target events.
  raw_ptr<optimization_guide::OptimizationTargetModelObserver> model_observer_;
};

class ClientSidePhishingModelTest : public content::RenderViewHostTestHarness {
 public:
  ClientSidePhishingModelTest() = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    model_observer_tracker_ =
        std::make_unique<ClientSidePhishingModelObserverTracker>();
    client_side_phishing_model_ = std::make_unique<ClientSidePhishingModel>(
        model_observer_tracker_.get());
  }

  void TearDown() override {
    content::RenderViewHostTestHarness::TearDown();
    client_side_phishing_model_.reset();
    model_observer_tracker_.reset();
  }

  void SendEmptyModelInfoUpdate(
      optimization_guide::proto::OptimizationTarget optimization_target) {
    model_observer_tracker_->SendEmptyModelInfoUpdate(optimization_target);
    task_environment()->RunUntilIdle();
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

  ClientSidePhishingModel* service() {
    return client_side_phishing_model_.get();
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  base::HistogramTester histogram_tester_;

  std::unique_ptr<ClientSidePhishingModelObserverTracker>
      model_observer_tracker_;
  std::unique_ptr<ClientSidePhishingModel> client_side_phishing_model_;
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

TEST_F(ClientSidePhishingModelTest, ValidModel) {
  base::FilePath model_file_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &model_file_path);
  model_file_path = model_file_path.AppendASCII("components")
                        .AppendASCII("test")
                        .AppendASCII("data")
                        .AppendASCII("safe_browsing")
                        .AppendASCII("client_model.pb");

  base::FilePath additional_files_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &additional_files_path);
  additional_files_path = additional_files_path.AppendASCII("components")
                              .AppendASCII("test")
                              .AppendASCII("data")
                              .AppendASCII("safe_browsing")

#if BUILDFLAG(IS_ANDROID)
                              .AppendASCII("visual_model_android.tflite");
#else
                              .AppendASCII("visual_model_desktop.tflite");
#endif
  service()->SetModelTypeForTesting(CSDModelType::kFlatbuffer);
  ValidateModel(model_file_path, {additional_files_path});

  histogram_tester().ExpectUniqueSample(
      "SBClientPhishing.ModelDynamicUpdateSuccess", true, 1);
  EXPECT_TRUE(service()->IsEnabled());

  // Image embedding model has not been loaded yet, so the versions are not
  // matching.
  EXPECT_FALSE(service()->IsModelMetadataImageEmbeddingVersionMatching());

  base::FilePath image_embedding_model_file_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
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

  EXPECT_TRUE(service()->HasImageEmbeddingModel());
  // The above must be true in order for below to be true.
  const base::File& image_embedding_file = service()->GetImageEmbeddingModel();
  EXPECT_TRUE(image_embedding_file.IsValid());
  EXPECT_TRUE(service()->IsModelMetadataImageEmbeddingVersionMatching());

  // We are going to load it another time to see that it works with a model
  // loaded already.
  ValidateImageEmbeddingModel(image_embedding_model_file_path);
  // Since we loaded the model twice, we expect this to be 2 now.
  histogram_tester().ExpectUniqueSample(
      "SBClientPhishing.ModelDynamicUpdateSuccess.ImageEmbedding", true, 2);

  EXPECT_TRUE(service()->HasImageEmbeddingModel());
  const base::File& image_embedding_file_2 =
      service()->GetImageEmbeddingModel();
  EXPECT_TRUE(image_embedding_file_2.IsValid());
  EXPECT_TRUE(service()->IsModelMetadataImageEmbeddingVersionMatching());

  // Now we're going to get rid of the image embedding model in file by sending
  // an empty model info.
  SendEmptyModelInfoUpdate(
      optimization_guide::proto::
          OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING_IMAGE_EMBEDDER);
  // Service should still be enabled because it can live without the image
  // embedding model.
  EXPECT_TRUE(service()->IsEnabled());
  EXPECT_FALSE(service()->HasImageEmbeddingModel());
  EXPECT_FALSE(service()->IsModelMetadataImageEmbeddingVersionMatching());
}

TEST_F(ClientSidePhishingModelTest, InvalidModelDueToInvalidPath) {
  base::ScopedTempDir model_dir;
  EXPECT_TRUE(model_dir.CreateUniqueTempDir());
  base::FilePath model_file_path =
      model_dir.GetPath().AppendASCII("non_existent_client_model.pb");
  base::WriteFile(model_file_path, "INVALID MODEL DATA");

  base::FilePath additional_files_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &additional_files_path);
  additional_files_path = additional_files_path.AppendASCII("components")
                              .AppendASCII("test")
                              .AppendASCII("data")
                              .AppendASCII("safe_browsing")

#if BUILDFLAG(IS_ANDROID)
                              .AppendASCII("visual_model_android.tflite");
#else
                              .AppendASCII("visual_model_desktop.tflite");
#endif

  ValidateModel(model_file_path, {additional_files_path});

  histogram_tester().ExpectUniqueSample(
      "SBClientPhishing.ModelDynamicUpdateSuccess", false, 1);
  EXPECT_FALSE(service()->IsEnabled());
}

TEST_F(ClientSidePhishingModelTest, InvalidModelDueToNonexistentPath) {
  base::ScopedTempDir model_dir;
  EXPECT_TRUE(model_dir.CreateUniqueTempDir());
  base::FilePath weird_model_file_path =
      model_dir.GetPath().AppendASCII("non_existent_directory/");

  base::FilePath additional_files_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &additional_files_path);
  additional_files_path = additional_files_path.AppendASCII("components")
                              .AppendASCII("test")
                              .AppendASCII("data")
                              .AppendASCII("safe_browsing")
#if BUILDFLAG(IS_ANDROID)
                              .AppendASCII("visual_model_android.tflite");
#else
                              .AppendASCII("visual_model_desktop.tflite");
#endif

  ValidateModel(weird_model_file_path, {additional_files_path});

  histogram_tester().ExpectUniqueSample(
      "SBClientPhishing.ModelDynamicUpdateSuccess", false, 1);
  EXPECT_FALSE(service()->IsEnabled());
}

TEST_F(ClientSidePhishingModelTest,
       InvalidModelDueToValidPathButMultipleAdditionalFilesPath) {
  base::ScopedTempDir model_dir;
  EXPECT_TRUE(model_dir.CreateUniqueTempDir());
  base::FilePath model_file_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &model_file_path);
  model_file_path = model_file_path.AppendASCII("components")
                        .AppendASCII("test")
                        .AppendASCII("data")
                        .AppendASCII("safe_browsing")
                        .AppendASCII("client_model.pb");

  base::FilePath additional_files_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &additional_files_path);
  additional_files_path = additional_files_path.AppendASCII("components")
                              .AppendASCII("test")
                              .AppendASCII("data")
                              .AppendASCII("safe_browsing")
#if BUILDFLAG(IS_ANDROID)
                              .AppendASCII("visual_model_android.tflite");
#else
                              .AppendASCII("visual_model_desktop.tflite");
#endif

  // It has to be different file name at the end for the set to count them
  // differently.
  base::FilePath additional_files_path2;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &additional_files_path2);
  additional_files_path2 = additional_files_path2.AppendASCII("components2")
                               .AppendASCII("test2")
                               .AppendASCII("data2")
                               .AppendASCII("safe_browsing2")

#if BUILDFLAG(IS_ANDROID)
                               .AppendASCII("visual_model_android2.tflite");
#else
                               .AppendASCII("visual_model_desktop2.tflite");
#endif

  ValidateModel(model_file_path,
                {additional_files_path, additional_files_path2});

  histogram_tester().ExpectUniqueSample(
      "SBClientPhishing.ModelDynamicUpdateSuccess", false, 1);
  EXPECT_FALSE(service()->IsEnabled());
}

TEST_F(ClientSidePhishingModelTest,
       InvalidImageEmbeddingModelDueToNonexistentPath) {
  base::ScopedTempDir model_dir;
  EXPECT_TRUE(model_dir.CreateUniqueTempDir());
  base::FilePath image_embedding_model_file_path =
      model_dir.GetPath().AppendASCII("non_existent_directory_2/");

  ValidateImageEmbeddingModel(image_embedding_model_file_path);

  histogram_tester().ExpectUniqueSample(
      "SBClientPhishing.ModelDynamicUpdateSuccess.ImageEmbedding", false, 1);
}

TEST_F(ClientSidePhishingModelTest,
       EmptyOptimizationGuideModelInfoDisablesService) {
  base::FilePath model_file_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &model_file_path);
  model_file_path = model_file_path.AppendASCII("components")
                        .AppendASCII("test")
                        .AppendASCII("data")
                        .AppendASCII("safe_browsing")
                        .AppendASCII("client_model.pb");

  base::FilePath additional_files_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &additional_files_path);
  additional_files_path = additional_files_path.AppendASCII("components")
                              .AppendASCII("test")
                              .AppendASCII("data")
                              .AppendASCII("safe_browsing")

#if BUILDFLAG(IS_ANDROID)
                              .AppendASCII("visual_model_android.tflite");
#else
                              .AppendASCII("visual_model_desktop.tflite");
#endif
  service()->SetModelTypeForTesting(CSDModelType::kFlatbuffer);
  ValidateModel(model_file_path, {additional_files_path});

  histogram_tester().ExpectUniqueSample(
      "SBClientPhishing.ModelDynamicUpdateSuccess", true, 1);
  EXPECT_TRUE(service()->IsEnabled());

  // It is enabled now, but we will send an empty model info update to the model
  // class, which should remove the model from the class, and notifying the
  // service class will make it disabled.
  SendEmptyModelInfoUpdate(
      optimization_guide::proto::OPTIMIZATION_TARGET_CLIENT_SIDE_PHISHING);

  EXPECT_FALSE(service()->IsEnabled());
}

TEST_F(ClientSidePhishingModelTest, RejectsInvalidFlatbuffer) {
  service()->SetVisualTfLiteModelForTesting(base::File());
  service()->ClearMappedRegionForTesting();
  service()->SetModelTypeForTesting(CSDModelType::kNone);
  service()->SetModelStringForTesting("bad flatbuffer", base::File());
  EXPECT_FALSE(service()->IsEnabled());
}

TEST_F(ClientSidePhishingModelTest, NotifiesForFile) {
  service()->SetVisualTfLiteModelForTesting(base::File());
  service()->ClearMappedRegionForTesting();
  service()->SetModelTypeForTesting(CSDModelType::kNone);

  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      service()->RegisterCallback(base::BindRepeating(
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
  const std::string_view file_contents = "visual model file";
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));

  const std::string model_str = CreateFlatBufferString();
  service()->SetModelStringForTesting(model_str, std::move(file));

  run_loop.Run();

  EXPECT_TRUE(called);
  EXPECT_TRUE(service()->IsEnabled());
}

TEST_F(ClientSidePhishingModelTest, DoesNotNotifyOnBadInitialUpdate) {
  service()->SetVisualTfLiteModelForTesting(base::File());
  service()->ClearMappedRegionForTesting();
  service()->SetModelTypeForTesting(CSDModelType::kNone);

  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      service()->RegisterCallback(base::BindRepeating(
          [](base::RepeatingClosure quit_closure, bool* called) {
            *called = true;
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), &called));

  service()->SetModelStringForTesting("", base::File());

  run_loop.RunUntilIdle();

  EXPECT_FALSE(called);
  EXPECT_FALSE(service()->IsEnabled());
}

TEST_F(ClientSidePhishingModelTest, DoesNotNotifyOnBadFollowingUpdate) {
  service()->SetVisualTfLiteModelForTesting(base::File());
  service()->ClearMappedRegionForTesting();
  service()->SetModelTypeForTesting(CSDModelType::kNone);

  base::RunLoop run_loop;

  // Perform a valid update.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("visual_model.tflite");
  base::File file(file_path, base::File::FLAG_OPEN_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  const std::string_view file_contents = "visual model file";
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));

  const std::string model_str = CreateFlatBufferString();
  service()->SetModelStringForTesting(model_str, std::move(file));

  run_loop.RunUntilIdle();

  // Perform an invalid update.
  bool called = false;
  base::CallbackListSubscription subscription =
      service()->RegisterCallback(base::BindRepeating(
          [](base::RepeatingClosure quit_closure, bool* called) {
            *called = true;
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), &called));

  service()->SetModelStringForTesting("", base::File());

  run_loop.RunUntilIdle();

  EXPECT_FALSE(called);
  EXPECT_TRUE(service()->IsEnabled());
}

TEST_F(ClientSidePhishingModelTest, CanOverrideFlatBufferWithFlag) {
  service()->SetVisualTfLiteModelForTesting(base::File());
  service()->ClearMappedRegionForTesting();
  service()->SetModelTypeForTesting(CSDModelType::kNone);

  base::test::ScopedCommandLine command_line;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath nonexistent_path =
      temp_dir.GetPath().AppendASCII("nonexistent");
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "csd-model-override-path", nonexistent_path);

  // Try overriding without setting anything in file path initially.
  service()->MaybeOverrideModel();
  EXPECT_EQ(service()->GetModelType(), CSDModelType::kNone);

  const base::FilePath empty_path;

  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "csd-model-override-path", empty_path);

  // Try overriding now with just an empty file path.
  service()->MaybeOverrideModel();
  EXPECT_EQ(service()->GetModelType(), CSDModelType::kNone);

  const base::FilePath file_path = temp_dir.GetPath();
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "csd-model-override-path", file_path);

  // Try overriding now with just a valid file path, but no files.
  service()->MaybeOverrideModel();
  EXPECT_EQ(service()->GetModelType(), CSDModelType::kNone);

  base::File file(file_path.AppendASCII("client_model.pb"),
                  base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                      base::File::FLAG_WRITE);

  const std::string file_contents = CreateFlatBufferString();
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));

  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      service()->RegisterCallback(base::BindRepeating(
          [](base::RepeatingClosure quit_closure, bool* called) {
            *called = true;
            std::move(quit_closure).Run();
          },
          run_loop.QuitClosure(), &called));

  // We now have everything, so we can expect the subscription to properly run
  // and the model will be set.
  service()->MaybeOverrideModel();

  run_loop.Run();

  std::string model_str_from_shared_mem;
  ASSERT_NO_FATAL_FAILURE(GetFlatBufferStringFromMappedMemory(
      service()->GetModelSharedMemoryRegion(), &model_str_from_shared_mem));
  EXPECT_EQ(model_str_from_shared_mem, file_contents);
  EXPECT_EQ(service()->GetModelType(), CSDModelType::kFlatbuffer);
  EXPECT_TRUE(called);
}

TEST_F(ClientSidePhishingModelTest, AcceptsValidFlatbuffer) {
  service()->SetVisualTfLiteModelForTesting(base::File());
  service()->ClearMappedRegionForTesting();
  service()->SetModelTypeForTesting(CSDModelType::kNone);

  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      service()->RegisterCallback(base::BindRepeating(
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
  const std::string_view file_contents = "visual model file";
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));
  service()->SetModelStringForTesting(model_str, std::move(file));
  run_loop.Run();

  EXPECT_TRUE(service()->IsEnabled());

  std::string model_str_from_shared_mem;
  ASSERT_NO_FATAL_FAILURE(GetFlatBufferStringFromMappedMemory(
      service()->GetModelSharedMemoryRegion(), &model_str_from_shared_mem));
  EXPECT_EQ(model_str, model_str_from_shared_mem);
  EXPECT_EQ(service()->GetModelType(), CSDModelType::kFlatbuffer);

  EXPECT_TRUE(called);
}

TEST_F(ClientSidePhishingModelTest, FlatbufferOnFollowingUpdate) {
  service()->SetVisualTfLiteModelForTesting(base::File());
  service()->ClearMappedRegionForTesting();
  service()->SetModelTypeForTesting(CSDModelType::kNone);

  base::RunLoop run_loop;

  const std::string model_str1 = CreateFlatBufferString();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("visual_model.tflite");
  base::File file(file_path, base::File::FLAG_OPEN_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  const std::string_view file_contents = "visual model file";
  file.WriteAtCurrentPos(base::as_byte_span(file_contents));
  service()->SetModelStringForTesting(model_str1, std::move(file));

  run_loop.RunUntilIdle();
  EXPECT_TRUE(service()->IsEnabled());

  std::string model_str_from_shared_mem1;

  ASSERT_NO_FATAL_FAILURE(GetFlatBufferStringFromMappedMemory(
      service()->GetModelSharedMemoryRegion(), &model_str_from_shared_mem1));
  EXPECT_EQ(model_str1, model_str_from_shared_mem1);
  EXPECT_EQ(service()->GetModelType(), CSDModelType::kFlatbuffer);
  // Should be able to write to memory with WritableSharedMemoryMapping field.
  void* memory_addr = service()->GetFlatBufferMemoryAddressForTesting();

  EXPECT_EQ(memset(memory_addr, 'G', 1), memory_addr);

  bool called = false;
  base::CallbackListSubscription subscription =
      service()->RegisterCallback(base::BindRepeating(
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
  const std::string_view file_contents2 = "visual model file";
  file2.WriteAtCurrentPos(base::as_byte_span(file_contents2));
  service()->SetModelStringForTesting(model_str2, std::move(file2));

  run_loop.RunUntilIdle();
  EXPECT_TRUE(called);
  EXPECT_TRUE(service()->IsEnabled());
  std::string model_str_from_shared_mem2;
  ASSERT_NO_FATAL_FAILURE(GetFlatBufferStringFromMappedMemory(
      service()->GetModelSharedMemoryRegion(), &model_str_from_shared_mem2));
  EXPECT_EQ(model_str2, model_str_from_shared_mem2);
  EXPECT_EQ(service()->GetModelType(), CSDModelType::kFlatbuffer);

  // Mapping should be undone automatically, even with a region copy lying
  // around.
  // Can remove this if flaky.
  // Windows ASAN flake: crbug.com/1234652
#if !(BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER))
  BASE_EXPECT_DEATH(memset(memory_addr, 'G', 1), "");
#endif
}

}  // namespace safe_browsing
