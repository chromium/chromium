// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_preview_feature.h"

#include <memory>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/origin_trials/origin_trials_factory.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::OriginTrialFeature;
using media_preview_metrics::UiLocation;
using testing::_;
using testing::Return;
using testing::StrictMock;

namespace content {
class BrowserContext;
}

class MockOriginTrialsDelegate
    : public content::OriginTrialsControllerDelegate {
 public:
  MOCK_METHOD(void,
              PersistTrialsFromTokens,
              (const url::Origin&,
               const url::Origin&,
               const base::span<const std::string>,
               const base::Time,
               std::optional<ukm::SourceId>),
              (override));
  MOCK_METHOD(void,
              PersistAdditionalTrialsFromTokens,
              (const url::Origin&,
               const url::Origin&,
               const base::span<const url::Origin>,
               const base::span<const std::string>,
               const base::Time,
               std::optional<ukm::SourceId>),
              (override));
  MOCK_METHOD(bool,
              IsFeaturePersistedForOrigin,
              (const url::Origin&,
               const url::Origin&,
               blink::mojom::OriginTrialFeature,
               const base::Time),
              (override));
  MOCK_METHOD(base::flat_set<std::string>,
              GetPersistedTrialsForOrigin,
              (const url::Origin&, const url::Origin&, const base::Time),
              (override));
  MOCK_METHOD(void, ClearPersistedTokens, (), (override));
};

class MediaPreviewsTestingProfile : public TestingProfile {
 public:
  explicit MediaPreviewsTestingProfile(MockOriginTrialsDelegate* delegate)
      : delegate_(delegate) {}
  ~MediaPreviewsTestingProfile() override { delegate_ = nullptr; }

  content::OriginTrialsControllerDelegate* GetOriginTrialsControllerDelegate()
      override {
    return delegate_;
  }

  void NullifyDelegate() { delegate_ = nullptr; }

 protected:
  raw_ptr<MockOriginTrialsDelegate> delegate_;
};

class MediaPreviewFeatureTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<MediaPreviewsTestingProfile>(&mock_delegate_);
    requester_url_ = GURL("https://www.example.com");
    embedder_url_ = GURL("https://www.iframe.com");
    url_in_ot_ = GURL("https://www.otvideoapp.com");
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<MediaPreviewsTestingProfile> profile_;
  StrictMock<MockOriginTrialsDelegate> mock_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
  GURL requester_url_;
  GURL embedder_url_;
  GURL url_in_ot_;
  GURL invalid_url_;
};

TEST_F(MediaPreviewFeatureTest, TestFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      blink::features::kCameraMicPreview);
  EXPECT_CALL(mock_delegate_, IsFeaturePersistedForOrigin(_, _, _, _)).Times(0);
  EXPECT_FALSE(media_preview_feature::ShouldShowMediaPreview(
      *profile_, requester_url_, embedder_url_, UiLocation::kPageInfo));
}

TEST_F(MediaPreviewFeatureTest, TestFeatureEnabledNoOriginTrial) {
  scoped_feature_list_.InitAndEnableFeature(blink::features::kCameraMicPreview);
  auto requester_origin = url::Origin::Create(requester_url_);
  auto embedder_origin = url::Origin::Create(embedder_url_);
  EXPECT_CALL(mock_delegate_, IsFeaturePersistedForOrigin(
                                  requester_origin, requester_origin,
                                  OriginTrialFeature::kMediaPreviewsOptOut, _))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_delegate_, IsFeaturePersistedForOrigin(
                                  embedder_origin, embedder_origin,
                                  OriginTrialFeature::kMediaPreviewsOptOut, _))
      .WillOnce(Return(false));
  EXPECT_TRUE(media_preview_feature::ShouldShowMediaPreview(
      *profile_, requester_url_, embedder_url_, UiLocation::kPageInfo));
}

TEST_F(MediaPreviewFeatureTest, TestFeatureEnabledInvalidUrls) {
  scoped_feature_list_.InitAndEnableFeature(blink::features::kCameraMicPreview);
  EXPECT_CALL(mock_delegate_, IsFeaturePersistedForOrigin(_, _, _, _)).Times(0);
  EXPECT_TRUE(media_preview_feature::ShouldShowMediaPreview(
      *profile_, invalid_url_, invalid_url_, UiLocation::kPageInfo));
}

TEST_F(MediaPreviewFeatureTest, TestFeatureEnabledNullDelegate) {
  scoped_feature_list_.InitAndEnableFeature(blink::features::kCameraMicPreview);
  auto url_in_ot_origin = url::Origin::Create(url_in_ot_);
  profile_->NullifyDelegate();
  EXPECT_TRUE(media_preview_feature::ShouldShowMediaPreview(
      *profile_, url_in_ot_, embedder_url_, UiLocation::kPageInfo));
}

TEST_F(MediaPreviewFeatureTest, TestFeatureEnabledRequestedInOriginTrial) {
  scoped_feature_list_.InitAndEnableFeature(blink::features::kCameraMicPreview);
  auto url_in_ot_origin = url::Origin::Create(url_in_ot_);
  EXPECT_CALL(mock_delegate_, IsFeaturePersistedForOrigin(
                                  url_in_ot_origin, url_in_ot_origin,
                                  OriginTrialFeature::kMediaPreviewsOptOut, _))
      .WillOnce(Return(true));
  EXPECT_FALSE(media_preview_feature::ShouldShowMediaPreview(
      *profile_, url_in_ot_, embedder_url_, UiLocation::kPageInfo));
}

TEST_F(MediaPreviewFeatureTest, TestFeatureEnabledEmbeddedInOriginTrial) {
  scoped_feature_list_.InitAndEnableFeature(blink::features::kCameraMicPreview);
  auto requester_origin = url::Origin::Create(requester_url_);
  auto url_in_ot_origin = url::Origin::Create(url_in_ot_);
  EXPECT_CALL(mock_delegate_, IsFeaturePersistedForOrigin(
                                  requester_origin, requester_origin,
                                  OriginTrialFeature::kMediaPreviewsOptOut, _))
      .WillOnce(Return(false));
  EXPECT_CALL(mock_delegate_, IsFeaturePersistedForOrigin(
                                  url_in_ot_origin, url_in_ot_origin,
                                  OriginTrialFeature::kMediaPreviewsOptOut, _))
      .WillOnce(Return(true));
  EXPECT_FALSE(media_preview_feature::ShouldShowMediaPreview(
      *profile_, requester_url_, url_in_ot_, UiLocation::kPageInfo));
}
