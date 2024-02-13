// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_head_provider.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "components/omnibox/browser/on_device_head_model.h"
#include "components/omnibox/browser/on_device_model_update_listener.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "components/optimization_guide/core/test_model_info_builder.h"
#endif

using testing::_;
using testing::NiceMock;
using testing::Return;

class OnDeviceHeadProviderTest : public testing::Test,
                                 public AutocompleteProviderListener {
 protected:
  void SetUp() override {
    client_ = std::make_unique<FakeAutocompleteProviderClient>();
    SetupTestOnDeviceHeadModel();
    provider_ = OnDeviceHeadProvider::Create(client_.get(), this);
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    provider_ = nullptr;
    client_.reset();
    task_environment_.RunUntilIdle();
  }

  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches,
                        const AutocompleteProvider* provider) override {
    // No action required.
  }

  void SetupTestOnDeviceHeadModel() {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
    // The same test model also used in ./on_device_head_model_unittest.cc.
    file_path = file_path.AppendASCII("components/test/data/omnibox");
    ASSERT_TRUE(base::PathExists(file_path));
    auto* update_listener = OnDeviceModelUpdateListener::GetInstance();
    if (update_listener)
      update_listener->OnHeadModelUpdate(file_path);
    task_environment_.RunUntilIdle();
  }

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  void SetupTestOnDeviceTailModel() {
    base::FilePath dir_path, tail_model_path, vocab_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir_path);
    dir_path = dir_path.AppendASCII("components/test/data/omnibox");
    // The same test model also used in
    // ./on_device_tail_model_executor_unittest.cc.
    tail_model_path = dir_path.AppendASCII("test_tail_model.tflite");
    ASSERT_TRUE(base::PathExists(tail_model_path));
    vocab_path = dir_path.AppendASCII("vocab_test.txt");
    ASSERT_TRUE(base::PathExists(vocab_path));

    base::flat_set<base::FilePath> additional_files;
    additional_files.insert(vocab_path);

    OnDeviceTailModelExecutor::ModelMetadata metadata;
    metadata.mutable_lstm_model_params()->set_num_layer(1);
    metadata.mutable_lstm_model_params()->set_state_size(512);
    metadata.mutable_lstm_model_params()->set_embedding_dimension(64);

    optimization_guide::proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/com.foo.OnDeviceTailSuggestModelMetadata");
    metadata.SerializeToString(any_metadata.mutable_value());

    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        optimization_guide::TestModelInfoBuilder()
            .SetModelFilePath(tail_model_path)
            .SetAdditionalFiles(additional_files)
            .SetVersion(123)
            .SetModelMetadata(any_metadata)
            .Build();

    client_->GetOnDeviceTailModelService()->OnModelUpdated(
        optimization_guide::proto::OptimizationTarget::
            OPTIMIZATION_TARGET_OMNIBOX_ON_DEVICE_TAIL_SUGGEST,
        *model_info);

    task_environment_.RunUntilIdle();
  }
#endif

  void ResetModelInstance() {
    auto* update_listener = OnDeviceModelUpdateListener::GetInstance();
    if (update_listener)
      update_listener->ResetListenerForTest();
  }

  bool IsOnDeviceHeadProviderAllowed(const AutocompleteInput& input) {
    return provider_->IsOnDeviceHeadProviderAllowed(input);
  }
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<OnDeviceHeadProvider> provider_;
};

TEST_F(OnDeviceHeadProviderTest, ModelInstanceNotCreated) {
  AutocompleteInput input(u"M", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_omit_asynchronous_matches(false);
  ResetModelInstance();

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(IsOnDeviceHeadProviderAllowed(input));

  provider_->Start(input, false);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(provider_->matches().empty());
  EXPECT_TRUE(provider_->done());
}

TEST_F(OnDeviceHeadProviderTest, RejectSynchronousRequest) {
  AutocompleteInput input(u"M", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_omit_asynchronous_matches(true);

  ASSERT_FALSE(IsOnDeviceHeadProviderAllowed(input));
}

TEST_F(OnDeviceHeadProviderTest, TestIfIncognitoIsAllowed) {
  AutocompleteInput input(u"M", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_omit_asynchronous_matches(false);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));

  // By default Incognito request will be accepted.
  {
    ASSERT_TRUE(IsOnDeviceHeadProviderAllowed(input));
  }
}

TEST_F(OnDeviceHeadProviderTest, RejectOnFocusRequest) {
  AutocompleteInput input(u"M", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_omit_asynchronous_matches(false);
  input.set_focus_type(metrics::OmniboxFocusType::INTERACTION_FOCUS);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled()).WillOnce(Return(true));

  ASSERT_FALSE(IsOnDeviceHeadProviderAllowed(input));
}

TEST_F(OnDeviceHeadProviderTest, NoMatches) {
  AutocompleteInput input(u"b", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_omit_asynchronous_matches(false);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(IsOnDeviceHeadProviderAllowed(input));

  provider_->Start(input, false);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(provider_->matches().empty());
  EXPECT_TRUE(provider_->done());
}

TEST_F(OnDeviceHeadProviderTest, HasHeadMatches) {
  AutocompleteInput input(u"M", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_omit_asynchronous_matches(false);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(IsOnDeviceHeadProviderAllowed(input));

  provider_->Start(input, false);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"maps", provider_->matches()[0].contents);
  EXPECT_EQ(u"mail", provider_->matches()[1].contents);
  EXPECT_EQ(u"map", provider_->matches()[2].contents);
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
TEST_F(OnDeviceHeadProviderTest, HasTailMatches) {
  SetupTestOnDeviceTailModel();
  AutocompleteInput input(u"Faceb", metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_omit_asynchronous_matches(false);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(IsOnDeviceHeadProviderAllowed(input));

  {
    SCOPED_TRACE("disable tail model for single word prefix");
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kOnDeviceTailModel, {
                                         {"EnableForSingleWordPrefix", "false"},
                                     });
    provider_->Start(input, false);
    task_environment_.RunUntilIdle();

    EXPECT_TRUE(provider_->done());
    EXPECT_TRUE(provider_->matches().empty());
  }

  {
    SCOPED_TRACE("enable tail model for single word prefix");
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        omnibox::kOnDeviceTailModel, {
                                         {"EnableForSingleWordPrefix", "true"},
                                     });
    provider_->Start(input, false);
    task_environment_.RunUntilIdle();

    EXPECT_TRUE(provider_->done());
    EXPECT_FALSE(provider_->matches().empty());
    EXPECT_TRUE(base::StartsWith(provider_->matches()[0].contents, u"facebook",
                                 base::CompareCase::SENSITIVE));
  }
}
#endif

TEST_F(OnDeviceHeadProviderTest, CancelInProgressRequest) {
  AutocompleteInput input1(u"g", metrics::OmniboxEventProto::OTHER,
                           TestSchemeClassifier());
  input1.set_omit_asynchronous_matches(false);
  AutocompleteInput input2(u"m", metrics::OmniboxEventProto::OTHER,
                           TestSchemeClassifier());
  input2.set_omit_asynchronous_matches(false);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(IsOnDeviceHeadProviderAllowed(input1));
  ASSERT_TRUE(IsOnDeviceHeadProviderAllowed(input2));

  provider_->Start(input1, false);
  provider_->Start(input2, false);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"maps", provider_->matches()[0].contents);
  EXPECT_EQ(u"mail", provider_->matches()[1].contents);
  EXPECT_EQ(u"map", provider_->matches()[2].contents);
}
