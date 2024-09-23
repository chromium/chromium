// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/about_this_site_service.h"

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/optimization_guide_decision.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/page_info/core/about_this_site_validation.h"
#include "components/page_info/core/features.h"
#include "components/page_info/core/proto/about_this_site_metadata.pb.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url_service.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace page_info {
using testing::_;
using testing::Invoke;
using testing::Return;

using about_this_site_validation::AboutThisSiteStatus;
using AboutThisSiteInteraction = AboutThisSiteService::AboutThisSiteInteraction;
using DecisionWithMetadata = AboutThisSiteService::DecisionAndMetadata;
using optimization_guide::OptimizationGuideDecision;
using optimization_guide::OptimizationMetadata;

class MockAboutThisSiteService : public AboutThisSiteService {
 public:
  MockAboutThisSiteService(TemplateURLService* template_url_service)
      : AboutThisSiteService(nullptr, false, nullptr, template_url_service) {}

  MOCK_METHOD(bool, IsOptimizationGuideAllowed, (), (const, override));
  MOCK_METHOD(optimization_guide::OptimizationGuideDecision,
              CanApplyOptimization,
              (const GURL&, OptimizationMetadata*),
              (const, override));
};

class MockTabHelper : public AboutThisSiteService::TabHelper {
 public:
  MOCK_METHOD(DecisionWithMetadata,
              GetAboutThisSiteMetadata,
              (),
              (const, override));
};

proto::AboutThisSiteMetadata CreateValidMetadata() {
  proto::AboutThisSiteMetadata metadata;
  auto* description = metadata.mutable_site_info()->mutable_description();
  description->set_description(
      "A domain used in illustrative examples in documents");
  description->set_lang("en_US");
  description->set_name("Example");
  description->mutable_source()->set_url("https://example.com");
  description->mutable_source()->set_label("Example source");
  metadata.mutable_site_info()->mutable_more_about()->set_url(
      "https://google.com/ats/example.com");
  return metadata;
}

proto::AboutThisSiteMetadata CreateInvalidDescription() {
  proto::AboutThisSiteMetadata metadata = CreateValidMetadata();
  metadata.mutable_site_info()->mutable_description()->clear_source();
  return metadata;
}

OptimizationGuideDecision ReturnDescription(const GURL& url,
                                            OptimizationMetadata* metadata) {
  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.AboutThisSiteMetadata");
  CreateValidMetadata().SerializeToString(any_metadata.mutable_value());
  metadata->set_any_metadata(any_metadata);
  return OptimizationGuideDecision::kTrue;
}

OptimizationGuideDecision ReturnInvalidDescription(
    const GURL& url,
    OptimizationMetadata* metadata) {
  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(
      "type.googleapis.com/com.foo.AboutThisSiteMetadata");
  CreateInvalidDescription().SerializeToString(any_metadata.mutable_value());
  metadata->set_any_metadata(any_metadata);
  return OptimizationGuideDecision::kTrue;
}

OptimizationGuideDecision ReturnNoResult(const GURL& url,
                                         OptimizationMetadata* metadata) {
  return OptimizationGuideDecision::kFalse;
}

OptimizationGuideDecision ReturnUnknown(const GURL& url,
                                        OptimizationMetadata* metadata) {
  return OptimizationGuideDecision::kUnknown;
}

class AboutThisSiteServiceTest : public ::testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    // Parameterize test until kAboutThisSiteAsyncFetching is enabled by
    // default.
    if (GetParam()) {
      tab_helper_mock_ = std::make_unique<testing::StrictMock<MockTabHelper>>();
    }

    service_ = std::make_unique<testing::StrictMock<MockAboutThisSiteService>>(
        search_engines_test_environment_.template_url_service());
    SetOptimizationGuideAllowed(true);
  }

  void SetOptimizationGuideAllowed(bool allowed) {
    EXPECT_CALL(*service(), IsOptimizationGuideAllowed())
        .WillRepeatedly(Return(allowed));
  }

  MockTabHelper* tab_helper() { return tab_helper_mock_.get(); }
  TemplateURLService* templateService() {
    return search_engines_test_environment_.template_url_service();
  }
  MockAboutThisSiteService* service() { return service_.get(); }

 private:
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::unique_ptr<MockAboutThisSiteService> service_;
  std::unique_ptr<MockTabHelper> tab_helper_mock_;
};

// Tests that correct proto messages are accepted.
TEST_P(AboutThisSiteServiceTest, ValidResponse) {
  base::HistogramTester t;
  if (GetParam()) {
    EXPECT_CALL(*tab_helper(), GetAboutThisSiteMetadata())
        .WillOnce(Return(DecisionWithMetadata{OptimizationGuideDecision::kTrue,
                                              CreateValidMetadata()}));
  } else {
    EXPECT_CALL(*service(), CanApplyOptimization(_, _))
        .WillOnce(Invoke(&ReturnDescription));
  }

  auto info = service()->GetAboutThisSiteInfo(
      GURL("https://foo.com"), ukm::UkmRecorder::GetNewSourceID(),
      tab_helper());
  EXPECT_TRUE(info.has_value());
  EXPECT_EQ(info->more_about().url(),
            "https://google.com/ats/example.com?ctx=chrome");
  t.ExpectUniqueSample("Security.PageInfo.AboutThisSiteStatus",
                       AboutThisSiteStatus::kValid, 1);
  t.ExpectUniqueSample("Security.PageInfo.AboutThisSiteInteraction",
                       AboutThisSiteInteraction::kShownWithDescription, 1);
}

// Tests the language specific feature check.
TEST_P(AboutThisSiteServiceTest, FeatureCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kPageInfoAboutThisSite);

  const char* enabled[]{"en-US", "en-UK",  "en", "pt", "pt-BR", "pt-PT",
                        "fr",    "fr-CA",  "it", "nl", "de",    "de-DE",
                        "es",    "es-419", "da", "id", "zh-TW", "ja"};
  const char* disabled[]{"gl", "si"};

  for (const char* lang : enabled) {
    EXPECT_TRUE(page_info::IsAboutThisSiteFeatureEnabled(lang));
  }
  for (const char* lang : disabled) {
    EXPECT_FALSE(page_info::IsAboutThisSiteFeatureEnabled(lang));
  }
}

// Tests that incorrect proto messages are discarded.
TEST_P(AboutThisSiteServiceTest, InvalidResponse) {
  base::HistogramTester t;
  if (GetParam()) {
    EXPECT_CALL(*tab_helper(), GetAboutThisSiteMetadata())
        .WillOnce(Return(DecisionWithMetadata{OptimizationGuideDecision::kTrue,
                                              CreateInvalidDescription()}));
  } else {
    EXPECT_CALL(*service(), CanApplyOptimization(_, _))
        .WillOnce(Invoke(&ReturnInvalidDescription));
  }

  auto info = service()->GetAboutThisSiteInfo(
      GURL("https://foo.com"), ukm::UkmRecorder::GetNewSourceID(),
      tab_helper());
  EXPECT_FALSE(info.has_value());
  t.ExpectUniqueSample("Security.PageInfo.AboutThisSiteStatus",
                       AboutThisSiteStatus::kMissingDescriptionSource, 1);
  t.ExpectUniqueSample("Security.PageInfo.AboutThisSiteInteraction",
                       AboutThisSiteInteraction::kNotShown, 1);
}

// Tests that no response is handled.
TEST_P(AboutThisSiteServiceTest, NoResponse) {
  base::HistogramTester t;
  std::optional<proto::AboutThisSiteMetadata> expected;
  if (GetParam()) {
    EXPECT_CALL(*tab_helper(), GetAboutThisSiteMetadata())
        .WillOnce(Return(DecisionWithMetadata{OptimizationGuideDecision::kFalse,
                                              std::nullopt}));
  } else {
    EXPECT_CALL(*service(), CanApplyOptimization(_, _))
        .WillOnce(Invoke(&ReturnNoResult));
  }

  auto info = service()->GetAboutThisSiteInfo(
      GURL("https://foo.com"), ukm::UkmRecorder::GetNewSourceID(),
      tab_helper());
  EXPECT_FALSE(info.has_value());
  t.ExpectUniqueSample("Security.PageInfo.AboutThisSiteStatus",
                       AboutThisSiteStatus::kNoResult, 1);
  t.ExpectUniqueSample("Security.PageInfo.AboutThisSiteInteraction",
                       AboutThisSiteInteraction::kNotShown, 1);
}

// Tests that unknown response is handled.
TEST_P(AboutThisSiteServiceTest, Unknown) {
  base::HistogramTester t;
  if (GetParam()) {
    EXPECT_CALL(*tab_helper(), GetAboutThisSiteMetadata())
        .WillOnce(Return(DecisionWithMetadata{
            OptimizationGuideDecision::kUnknown, std::nullopt}));
  } else {
    EXPECT_CALL(*service(), CanApplyOptimization(_, _))
        .WillOnce(Invoke(&ReturnUnknown));
  }

  auto info = service()->GetAboutThisSiteInfo(
      GURL("https://foo.com"), ukm::UkmRecorder::GetNewSourceID(),
      tab_helper());
  EXPECT_FALSE(info.has_value());
  t.ExpectUniqueSample("Security.PageInfo.AboutThisSiteStatus",
                       AboutThisSiteStatus::kUnknown, 1);
  t.ExpectUniqueSample("Security.PageInfo.AboutThisSiteInteraction",
                       AboutThisSiteInteraction::kNotShown, 1);
}

// Tests that ATP not shown when Google is not set as DSE
TEST_P(AboutThisSiteServiceTest, NotShownWhenNoGoogleDSE) {
  base::HistogramTester t;

  // Changing default provider to other than Google
  TemplateURL* template_url =
      templateService()->Add(std::make_unique<TemplateURL>(TemplateURLData(
          u"shortname", u"keyword", "https://cs.chromium.org",
          std::string_view(), std::string_view(), std::string_view(),
          std::string_view(), std::string_view(), std::string_view(),
          std::string_view(), std::string_view(), std::string_view(),
          std::string_view(), std::string_view(), std::string_view(),
          std::string_view(), std::string_view(), std::vector<std::string>(),
          std::string_view(), std::string_view(), std::u16string_view(),
          base::Value::List(), false, false, 0,
          base::span<TemplateURLData::RegulatoryExtension>())));
  templateService()->SetUserSelectedDefaultSearchProvider(template_url);

  auto info = service()->GetAboutThisSiteInfo(
      GURL("https://foo.com"), ukm::UkmRecorder::GetNewSourceID(),
      tab_helper());
  EXPECT_FALSE(info.has_value());

  t.ExpectTotalCount("Security.PageInfo.AboutThisSiteStatus", 0);
  t.ExpectUniqueSample("Security.PageInfo.AboutThisSiteInteraction",
                       AboutThisSiteInteraction::kNotShownNonGoogleDSE, 1);
}

// Tests that IP addresses and localhost are handled.
TEST_P(AboutThisSiteServiceTest, LocalHosts) {
  base::HistogramTester t;

  auto info = service()->GetAboutThisSiteInfo(
      GURL("https://localhost"), ukm::UkmRecorder::GetNewSourceID(),
      tab_helper());
  EXPECT_FALSE(info.has_value());
  info = service()->GetAboutThisSiteInfo(GURL("https://127.0.0.1"),
                                         ukm::UkmRecorder::GetNewSourceID(),
                                         tab_helper());
  EXPECT_FALSE(info.has_value());
  info = service()->GetAboutThisSiteInfo(GURL("https://192.168.0.1"),
                                         ukm::UkmRecorder::GetNewSourceID(),
                                         tab_helper());
  EXPECT_FALSE(info.has_value());

  t.ExpectTotalCount("Security.PageInfo.AboutThisSiteStatus", 0);
  t.ExpectUniqueSample("Security.PageInfo.AboutThisSiteInteraction",
                       AboutThisSiteInteraction::kNotShownLocalHost, 3);
}

// Tests the local creation of the Diner URL for navigation.
TEST_P(AboutThisSiteServiceTest, CreateMoreAboutUrlForNavigation) {
  auto url =
      service()->CreateMoreAboutUrlForNavigation(GURL("https://foo.com"));
  EXPECT_EQ(url,
            "https://www.google.com/search?"
            "q=About+https%3A%2F%2Ffoo.com%2F"
            "&tbm=ilp&ctx=chrome_nav");
}

// Tests the local creation of the Diner URL for navigation with anchor.
TEST_P(AboutThisSiteServiceTest, CreateMoreAboutUrlForNavigationWithAnchor) {
  auto url = service()->CreateMoreAboutUrlForNavigation(
      GURL("https://foo.com#anchor"));
  EXPECT_EQ(url,
            "https://www.google.com/search?"
            "q=About+https%3A%2F%2Ffoo.com%2F%23anchor"
            "&tbm=ilp&ctx=chrome_nav");
}

// Tests the local creation of the Diner URL for navigation from an origin with
// path.
TEST_P(AboutThisSiteServiceTest, CreateMoreAboutUrlForNavigationWithPath) {
  auto url = service()->CreateMoreAboutUrlForNavigation(
      GURL("https://foo.com/index.html"));
  EXPECT_EQ(url,
            "https://www.google.com/search?"
            "q=About+https%3A%2F%2Ffoo.com%2Findex.html"
            "&tbm=ilp&ctx=chrome_nav");
}

// Tests the local creation of the Diner URL for navigation from an invalid
// origin.
TEST_P(AboutThisSiteServiceTest, CreateMoreAboutUrlForNavigationInvalid) {
  auto url = service()->CreateMoreAboutUrlForNavigation(
      GURL("https://127.0.0.1/index.html"));
  EXPECT_EQ(url,
            "https://www.google.com/search?"
            "q=About+https%3A%2F%2F127.0.0.1%2F"
            "&tbm=ilp&ctx=chrome_nav");
}

// Tests the local creation of the Diner URL for navigation from an invalid
// origin (blank).
TEST_P(AboutThisSiteServiceTest, CreateMoreAboutUrlForNavigationInvalidBlank) {
  auto url = service()->CreateMoreAboutUrlForNavigation(GURL("about:blank"));
  EXPECT_EQ(url,
            "https://www.google.com/search?"
            "q=About+"
            "&tbm=ilp&ctx=chrome_nav");
}

// Tests the local creation of the Diner URL for navigation from an invalid
// origin (file).
TEST_P(AboutThisSiteServiceTest, CreateMoreAboutUrlForNavigationInvalidFile) {
  auto url = service()->CreateMoreAboutUrlForNavigation(GURL("file:///a/b/c"));
  EXPECT_EQ(url,
            "https://www.google.com/search?"
            "q=About+file%3A%2F%2F%2F"
            "&tbm=ilp&ctx=chrome_nav");
}

// Test with TabHelper based fetching enabled and disabled.
INSTANTIATE_TEST_SUITE_P(/* no label */,
                         AboutThisSiteServiceTest,
                         testing::Bool());

}  // namespace page_info
