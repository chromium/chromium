// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_phishing_model.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/safe_browsing/core/fbs/client_model_generated.h"
#include "components/safe_browsing/core/features.h"
#include "components/safe_browsing/core/proto/client_model.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

void ResetClientSidePhishingModel() {
  ClientSidePhishingModel::GetInstance()->SetModelStrForTesting("");
  ClientSidePhishingModel::GetInstance()->SetVisualTfLiteModelForTesting(
      base::File());
}

}  // namespace

TEST(ClientSidePhishingModelTest, NotifiesOnUpdate) {
  ResetClientSidePhishingModel();

  content::BrowserTaskEnvironment task_environment;
  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      ClientSidePhishingModel::GetInstance()->RegisterCallback(
          base::BindRepeating(
              [](base::RepeatingClosure quit_closure, bool* called) {
                *called = true;
                std::move(quit_closure).Run();
              },
              run_loop.QuitClosure(), &called));

  ClientSideModel model;
  model.set_max_words_per_term(0);  // Required field.
  ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
      model.SerializeAsString(), base::File());

  run_loop.Run();

  EXPECT_TRUE(called);
  EXPECT_EQ(model.SerializeAsString(),
            ClientSidePhishingModel::GetInstance()->GetModelStr());
  EXPECT_TRUE(ClientSidePhishingModel::GetInstance()->IsEnabled());
}

TEST(ClientSidePhishingModelTest, RejectsInvalidProto) {
  ResetClientSidePhishingModel();
  ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
      "bad proto", base::File());
  EXPECT_FALSE(ClientSidePhishingModel::GetInstance()->IsEnabled());
}

TEST(ClientSidePhishingModelTest, RejectsInvalidFlatbuffer) {
  ResetClientSidePhishingModel();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kClientSideDetectionModelIsFlatBuffer},
      /*disabled_features=*/{});
  ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
      "bad flatbuffer", base::File());
  EXPECT_FALSE(ClientSidePhishingModel::GetInstance()->IsEnabled());
}

TEST(ClientSidePhishingModelTest, NotifiesForFile) {
  ResetClientSidePhishingModel();

  content::BrowserTaskEnvironment task_environment;
  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      ClientSidePhishingModel::GetInstance()->RegisterCallback(
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

  ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
      "", std::move(file));

  run_loop.Run();

  EXPECT_TRUE(called);
  EXPECT_TRUE(ClientSidePhishingModel::GetInstance()->IsEnabled());
}

TEST(ClientSidePhishingModelTest, DoesNotNotifyOnBadInitialUpdate) {
  ResetClientSidePhishingModel();

  content::BrowserTaskEnvironment task_environment;
  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      ClientSidePhishingModel::GetInstance()->RegisterCallback(
          base::BindRepeating(
              [](base::RepeatingClosure quit_closure, bool* called) {
                *called = true;
                std::move(quit_closure).Run();
              },
              run_loop.QuitClosure(), &called));

  ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
      "", base::File());

  run_loop.RunUntilIdle();

  EXPECT_FALSE(called);
  EXPECT_FALSE(ClientSidePhishingModel::GetInstance()->IsEnabled());
}

TEST(ClientSidePhishingModelTest, DoesNotNotifyOnBadFollowingUpdate) {
  ResetClientSidePhishingModel();

  content::BrowserTaskEnvironment task_environment;
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

  ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
      "", std::move(file));

  run_loop.RunUntilIdle();

  // Perform an invalid update.
  bool called = false;
  base::CallbackListSubscription subscription =
      ClientSidePhishingModel::GetInstance()->RegisterCallback(
          base::BindRepeating(
              [](base::RepeatingClosure quit_closure, bool* called) {
                *called = true;
                std::move(quit_closure).Run();
              },
              run_loop.QuitClosure(), &called));

  ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
      "", base::File());

  run_loop.RunUntilIdle();

  EXPECT_FALSE(called);
  EXPECT_TRUE(ClientSidePhishingModel::GetInstance()->IsEnabled());
}

TEST(ClientSidePhishingModelTest, CanOverrideProtoWithFlag) {
  ResetClientSidePhishingModel();
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("overridden_model.proto");
  base::File file(file_path, base::File::FLAG_OPEN_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);
  ClientSideModel model_proto;
  model_proto.set_version(123);
  model_proto.set_max_words_per_term(0);  // Required field
  const std::string file_contents = model_proto.SerializeAsString();
  file.WriteAtCurrentPos(file_contents.data(), file_contents.size());

  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "--csd-model-override-path", file_path);

  content::BrowserTaskEnvironment task_environment;
  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      ClientSidePhishingModel::GetInstance()->RegisterCallback(
          base::BindRepeating(
              [](base::RepeatingClosure quit_closure, bool* called) {
                *called = true;
                std::move(quit_closure).Run();
              },
              run_loop.QuitClosure(), &called));

  ClientSidePhishingModel::GetInstance()->MaybeOverrideModel();

  run_loop.Run();

  EXPECT_EQ(ClientSidePhishingModel::GetInstance()->GetModelStr(),
            file_contents);
  EXPECT_EQ(ClientSidePhishingModel::GetInstance()->GetModelType(),
            CSDModelType::kProtobuf);
  EXPECT_TRUE(called);
}

TEST(ClientSidePhishingModelTest, CanOverrideFlatBufferWithFlag) {
  ResetClientSidePhishingModel();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kClientSideDetectionModelIsFlatBuffer},
      /*disabled_features=*/{});
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath file_path =
      temp_dir.GetPath().AppendASCII("overridden_model.fb");
  base::File file(file_path, base::File::FLAG_OPEN_ALWAYS |
                                 base::File::FLAG_READ |
                                 base::File::FLAG_WRITE);

  flatbuffers::FlatBufferBuilder builder(1024);
  flat::ClientSideModelBuilder csd_model_builder(builder);
  csd_model_builder.add_version(123);
  builder.Finish(csd_model_builder.Finish());
  const std::string file_contents(
      reinterpret_cast<char*>(builder.GetBufferPointer()), builder.GetSize());
  file.WriteAtCurrentPos(file_contents.data(), file_contents.size());

  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchPath(
      "csd-model-override-path", file_path);

  content::BrowserTaskEnvironment task_environment;
  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      ClientSidePhishingModel::GetInstance()->RegisterCallback(
          base::BindRepeating(
              [](base::RepeatingClosure quit_closure, bool* called) {
                *called = true;
                std::move(quit_closure).Run();
              },
              run_loop.QuitClosure(), &called));

  ClientSidePhishingModel::GetInstance()->MaybeOverrideModel();

  run_loop.Run();

  EXPECT_EQ(ClientSidePhishingModel::GetInstance()->GetModelStr(),
            file_contents);
  EXPECT_EQ(ClientSidePhishingModel::GetInstance()->GetModelType(),
            CSDModelType::kFlatbuffer);
  EXPECT_TRUE(called);
}

TEST(ClientSidePhishingModelTest, AcceptsValidFlatbufferIfFeatureEnabled) {
  ResetClientSidePhishingModel();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{kClientSideDetectionModelIsFlatBuffer},
      /*disabled_features=*/{});
  content::BrowserTaskEnvironment task_environment;
  base::RunLoop run_loop;
  bool called = false;
  base::CallbackListSubscription subscription =
      ClientSidePhishingModel::GetInstance()->RegisterCallback(
          base::BindRepeating(
              [](base::RepeatingClosure quit_closure, bool* called) {
                *called = true;
                std::move(quit_closure).Run();
              },
              run_loop.QuitClosure(), &called));

  flatbuffers::FlatBufferBuilder builder(1024);
  flat::ClientSideModelBuilder csd_model_builder(builder);
  builder.Finish(csd_model_builder.Finish());
  const std::string model_str(
      reinterpret_cast<char*>(builder.GetBufferPointer()), builder.GetSize());
  ClientSidePhishingModel::GetInstance()->PopulateFromDynamicUpdate(
      model_str, base::File());
  EXPECT_TRUE(ClientSidePhishingModel::GetInstance()->IsEnabled());
  EXPECT_EQ(model_str, ClientSidePhishingModel::GetInstance()->GetModelStr());

  run_loop.Run();
  EXPECT_TRUE(called);
}

}  // namespace safe_browsing
