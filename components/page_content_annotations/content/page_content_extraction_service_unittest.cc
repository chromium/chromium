// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_content_extraction_service.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_content_annotations {

using testing::_;
using testing::Return;

class PageContentExtractionServiceTest : public testing::Test {
 public:
  PageContentExtractionServiceTest() = default;
  ~PageContentExtractionServiceTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_dir_;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  feature_engagement::test::MockTracker mock_tracker_;
};

TEST_F(PageContentExtractionServiceTest, CacheDisabled) {
  scoped_feature_list_.InitAndDisableFeature(features::kPageContentCache);

  PageContentExtractionService service(os_crypt_async_.get(),
                                       temp_dir_.GetPath(), &mock_tracker_);

  EXPECT_FALSE(service.IsOnDiskCacheEnabled());
}

TEST_F(PageContentExtractionServiceTest, CacheEnabled_NoEngagement) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kPageContentCache,
      {{"page_content_cache_use_user_engagement", "false"}});

  PageContentExtractionService service(os_crypt_async_.get(),
                                       temp_dir_.GetPath(), &mock_tracker_);

  EXPECT_TRUE(service.IsOnDiskCacheEnabled());
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(PageContentExtractionServiceTest,
       CacheEnabled_Engagement_ShouldTrigger) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kPageContentCache,
      {{"page_content_cache_use_user_engagement", "true"}});

  EXPECT_CALL(mock_tracker_,
              WouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHFuseboxAttachmentFeature)))
      .WillOnce(Return(true));

  PageContentExtractionService service(os_crypt_async_.get(),
                                       temp_dir_.GetPath(), &mock_tracker_);

  EXPECT_TRUE(service.IsOnDiskCacheEnabled());
}

TEST_F(PageContentExtractionServiceTest,
       CacheEnabled_Engagement_ShouldNotTrigger) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kPageContentCache,
      {{"page_content_cache_use_user_engagement", "true"}});

  EXPECT_CALL(mock_tracker_,
              WouldTriggerHelpUI(testing::Ref(
                  feature_engagement::kIPHFuseboxAttachmentFeature)))
      .WillOnce(Return(false));

  PageContentExtractionService service(os_crypt_async_.get(),
                                       temp_dir_.GetPath(), &mock_tracker_);

  EXPECT_FALSE(service.IsOnDiskCacheEnabled());
}
#endif  // BUILDFLAG(IS_ANDROID)

class TestObserver : public PageContentExtractionService::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;
};

TEST_F(PageContentExtractionServiceTest, OnNewNavigation_Metrics) {
  PageContentExtractionService service(os_crypt_async_.get(),
                                       temp_dir_.GetPath(), &mock_tracker_);

  // Case 1: Feature disabled, no observers -> kNone
  {
    base::HistogramTester histogram_tester;
    scoped_feature_list_.InitAndDisableFeature(
        features::kAnnotatedPageContentExtraction);

    service.OnNewNavigation(std::nullopt, nullptr, /*is_same_document=*/false);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentExtraction.EnablementSourcePerNavigation",
        0, 1);  // kNone
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentExtraction.ObserverCountPerNavigation", 0,
        1);
  }

  // Case 2: Feature enabled, no observers -> kFeatureFlag
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kAnnotatedPageContentExtraction);

    service.OnNewNavigation(std::nullopt, nullptr, /*is_same_document=*/false);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentExtraction.EnablementSourcePerNavigation",
        1, 1);  // kFeatureFlag
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentExtraction.ObserverCountPerNavigation", 0,
        1);
  }

  // Case 3: Feature disabled, with observer -> kObserverPresent
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kAnnotatedPageContentExtraction);

    TestObserver observer;
    service.AddObserver(&observer);

    service.OnNewNavigation(std::nullopt, nullptr, /*is_same_document=*/false);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentExtraction.EnablementSourcePerNavigation",
        2, 1);  // kObserverPresent
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentExtraction.ObserverCountPerNavigation", 1,
        1);

    service.RemoveObserver(&observer);
  }

  // Case 4: Feature enabled, with observer -> kBoth
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kAnnotatedPageContentExtraction);

    TestObserver observer;
    service.AddObserver(&observer);

    service.OnNewNavigation(std::nullopt, nullptr, /*is_same_document=*/false);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentExtraction.EnablementSourcePerNavigation",
        3, 1);  // kBoth
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentExtraction.ObserverCountPerNavigation", 1,
        1);

    service.RemoveObserver(&observer);
  }

  // Case 5: Same document navigation -> No logging
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        features::kAnnotatedPageContentExtraction);

    service.OnNewNavigation(std::nullopt, nullptr, /*is_same_document=*/true);

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.PageContentExtraction.EnablementSourcePerNavigation",
        0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.PageContentExtraction.ObserverCountPerNavigation",
        0);
  }

  // Case 6: Multiple observers -> count is correct
  {
    base::HistogramTester histogram_tester;
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kAnnotatedPageContentExtraction);

    TestObserver observer1;
    TestObserver observer2;
    TestObserver observer3;
    service.AddObserver(&observer1);
    service.AddObserver(&observer2);
    service.AddObserver(&observer3);

    service.OnNewNavigation(std::nullopt, nullptr, /*is_same_document=*/false);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentExtraction.EnablementSourcePerNavigation",
        2, 1);  // kObserverPresent
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentExtraction.ObserverCountPerNavigation", 3,
        1);

    service.RemoveObserver(&observer1);
    service.RemoveObserver(&observer2);
    service.RemoveObserver(&observer3);
  }
}

}  // namespace page_content_annotations
