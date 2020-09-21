// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/on_device_head_provider.h"

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
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/omnibox_focus_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;
using testing::Return;

class OnDeviceHeadProviderTest : public testing::Test,
                                 public AutocompleteProviderListener {
 protected:
  void SetUp() override {
    client_.reset(new FakeAutocompleteProviderClient());
    SetTestOnDeviceHeadModel();
    provider_ = OnDeviceHeadProvider::Create(client_.get(), this);
    provider_->AddModelUpdateCallback();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    provider_ = nullptr;
    client_.reset();
    task_environment_.RunUntilIdle();
  }

  // AutocompleteProviderListener:
  void OnProviderUpdate(bool updated_matches) override {
    // No action required.
  }

  void SetTestOnDeviceHeadModel() {
    base::FilePath file_path;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
    // The same test model also used in ./on_device_head_model_unittest.cc.
    file_path = file_path.AppendASCII("components/test/data/omnibox");
    ASSERT_TRUE(base::PathExists(file_path));
    auto* update_listener = OnDeviceModelUpdateListener::GetInstance();
    if (update_listener)
      update_listener->OnModelUpdate(file_path);
    task_environment_.RunUntilIdle();
  }

  void ResetModelInstance() {
    if (provider_) {
      provider_->model_filename_.clear();
    }
  }

  bool IsOnDeviceHeadProviderAllowed(const AutocompleteInput& input) {
    return provider_->IsOnDeviceHeadProviderAllowed(input);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<OnDeviceHeadProvider> provider_;
};

TEST_F(OnDeviceHeadProviderTest, ModelInstanceNotCreated) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kOnDeviceHeadProviderNonIncognito,
      {{OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs, "0"}});
  AutocompleteInput input(base::UTF8ToUTF16("M"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_want_asynchronous_matches(true);
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
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kOnDeviceHeadProviderNonIncognito,
      {{OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs, "0"}});
  AutocompleteInput input(base::UTF8ToUTF16("M"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_want_asynchronous_matches(false);

  ASSERT_FALSE(IsOnDeviceHeadProviderAllowed(input));
}

TEST_F(OnDeviceHeadProviderTest, TestIfIncognitoIsAllowed) {
  AutocompleteInput input(base::UTF8ToUTF16("M"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_want_asynchronous_matches(true);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(true));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));

  // By default incognito request will be rejected.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        omnibox::kOnDeviceHeadProviderNonIncognito);
    ASSERT_FALSE(IsOnDeviceHeadProviderAllowed(input));
  }

  // Now enable for incognito.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        omnibox::kOnDeviceHeadProviderIncognito);
    ASSERT_TRUE(IsOnDeviceHeadProviderAllowed(input));
  }

  // Test enable for both.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {omnibox::kOnDeviceHeadProviderNonIncognito,
         omnibox::kOnDeviceHeadProviderIncognito},
        {});
    ASSERT_TRUE(IsOnDeviceHeadProviderAllowed(input));
  }

  // Disable omnibox::kNewSearchFeatures and now all modes should be disabled.
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {omnibox::kOnDeviceHeadProviderNonIncognito,
         omnibox::kOnDeviceHeadProviderIncognito},
        {omnibox::kNewSearchFeatures});
    ASSERT_FALSE(IsOnDeviceHeadProviderAllowed(input));
  }
}

TEST_F(OnDeviceHeadProviderTest, RejectOnFocusRequest) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kOnDeviceHeadProviderNonIncognito,
      {{OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs, "0"}});
  AutocompleteInput input(base::UTF8ToUTF16("M"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_want_asynchronous_matches(true);
  input.set_focus_type(OmniboxFocusType::ON_FOCUS);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled()).WillOnce(Return(true));

  ASSERT_FALSE(IsOnDeviceHeadProviderAllowed(input));
}

TEST_F(OnDeviceHeadProviderTest, NoMatches) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kOnDeviceHeadProviderNonIncognito,
      {{OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs, "0"}});
  AutocompleteInput input(base::UTF8ToUTF16("b"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_want_asynchronous_matches(true);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(IsOnDeviceHeadProviderAllowed(input));

  provider_->Start(input, false);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(provider_->matches().empty());
  EXPECT_TRUE(provider_->done());
}

TEST_F(OnDeviceHeadProviderTest, HasMatches) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kOnDeviceHeadProviderNonIncognito,
      {{OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs, "0"}});
  AutocompleteInput input(base::UTF8ToUTF16("M"),
                          metrics::OmniboxEventProto::OTHER,
                          TestSchemeClassifier());
  input.set_want_asynchronous_matches(true);

  EXPECT_CALL(*client_.get(), IsOffTheRecord()).WillRepeatedly(Return(false));
  EXPECT_CALL(*client_.get(), SearchSuggestEnabled())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(IsOnDeviceHeadProviderAllowed(input));

  provider_->Start(input, false);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(provider_->done());
  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(base::UTF8ToUTF16("maps"), provider_->matches()[0].contents);
  EXPECT_EQ(base::UTF8ToUTF16("mail"), provider_->matches()[1].contents);
  EXPECT_EQ(base::UTF8ToUTF16("map"), provider_->matches()[2].contents);
}

TEST_F(OnDeviceHeadProviderTest, CancelInProgressRequest) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      omnibox::kOnDeviceHeadProviderNonIncognito,
      {{OmniboxFieldTrial::kOnDeviceHeadSuggestDelaySuggestRequestMs, "0"}});
  AutocompleteInput input1(base::UTF8ToUTF16("g"),
                           metrics::OmniboxEventProto::OTHER,
                           TestSchemeClassifier());
  input1.set_want_asynchronous_matches(true);
  AutocompleteInput input2(base::UTF8ToUTF16("m"),
                           metrics::OmniboxEventProto::OTHER,
                           TestSchemeClassifier());
  input2.set_want_asynchronous_matches(true);

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
  EXPECT_EQ(base::UTF8ToUTF16("maps"), provider_->matches()[0].contents);
  EXPECT_EQ(base::UTF8ToUTF16("mail"), provider_->matches()[1].contents);
  EXPECT_EQ(base::UTF8ToUTF16("map"), provider_->matches()[2].contents);
}
