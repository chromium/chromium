// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "components/component_updater/installer_policies/url_param_classification_component_installer.h"
#include "components/url_param_filter/core/features.h"
#include "components/url_param_filter/core/url_param_classifications_loader.h"
#include "components/url_param_filter/core/url_param_filter_test_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

namespace url_param_filter {

namespace {

enum ShouldFilterState { kUnset, kTrue, kFalse };

constexpr char DEFAULT_TAG[] = "default";

std::string ShouldFilterStateToString(ShouldFilterState state) {
  switch (state) {
    case ShouldFilterState::kUnset:
      return "";
    case ShouldFilterState::kTrue:
      return "true";
    case ShouldFilterState::kFalse:
      return "false";
  }
}

}  // namespace

// A base class for other classes to extend.
//
// Several subclasses must be created in order to test the ClassificationsLoader
// since it relies on the kIncognitoParamFilterEnabled base::Feature and the
// UrlParamClassification Component, and these must be instantiated within
// SetUpInProcessBrowserTextFixture() rather than from within a test.
class ClassificationsLoaderBrowserTest : public InProcessBrowserTest {
 public:
  ClassificationsLoaderBrowserTest() = default;

 protected:
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    base::CommandLine default_command_line(base::CommandLine::NO_PROGRAM);
    InProcessBrowserTest::SetUpDefaultCommandLine(&default_command_line);
    // Remove this switch to allow components to be updated.
    test_launcher_utils::RemoveCommandLineSwitch(
        default_command_line, switches::kDisableComponentUpdate, command_line);
  }

  void TearDown() override {
    ClassificationsLoader::GetInstance()->ResetListsForTesting();
    component_updater::UrlParamClassificationComponentInstallerPolicy::
        ResetForTesting();
    InProcessBrowserTest::TearDown();
  }

  ClassificationsLoader* loader() {
    return ClassificationsLoader::GetInstance();
  }

  base::test::ScopedFeatureList feature_list_;
  base::ScopedTempDir component_dir_;
};

// Feature fully disabled, component not installed.
class ClassificationsLoaderFeatureDisabledAndComponentNotInstalled
    : public ClassificationsLoaderBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndDisableFeature(features::kIncognitoParamFilterEnabled);
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(
    ClassificationsLoaderFeatureDisabledAndComponentNotInstalled,
    FeatureDisabled_NoClassificationsLoaded) {
  // ClassificationLoader has no classifications since the feature is
  // disabled.
  EXPECT_THAT(loader()->GetClassifications(), IsEmpty());
}

// Feature fully disabled, component installed.
class ClassificationsLoaderFeatureDisabledAndComponentInstalled
    : public ClassificationsLoaderBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndDisableFeature(features::kIncognitoParamFilterEnabled);
    // Force install the component.
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;

    component_updater::UrlParamClassificationComponentInstallerPolicy::
        WriteComponentForTesting(
            component_dir_.GetPath(),
            CreateSerializedUrlParamFilterClassificationForTesting(
                {{"source.test", {"plzblock_src"}}},
                {{"dest.test", {"plzblock_dest"}}}, {DEFAULT_TAG}));
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(
    ClassificationsLoaderFeatureDisabledAndComponentInstalled,
    FeatureDisabled_NoClassificationsLoaded) {
  // ClassificationLoader has no classifications since the feature is
  // disabled.
  EXPECT_THAT(loader()->GetClassifications(), IsEmpty());
}

// Feature enabled without params, component not installed.
class ClassificationsLoaderFeatureEnabledAndComponentNotInstalled
    : public ClassificationsLoaderBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeature(features::kIncognitoParamFilterEnabled);
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(
    ClassificationsLoaderFeatureEnabledAndComponentNotInstalled,
    NeitherSourceProvidesClassifications_NoClassificationsLoaded) {
  // ClassificationLoader has no classifications since neither feature
  // classifications nor Component Updater classifications were provided.
  EXPECT_THAT(loader()->GetClassifications(), IsEmpty());
}

// Feature enabled without params, component installed.
class ClassificationsLoaderFeatureEnabledAndComponentInstalled
    : public ClassificationsLoaderBrowserTest {
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeature(features::kIncognitoParamFilterEnabled);
    // Force install the component.
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;

    component_updater::UrlParamClassificationComponentInstallerPolicy::
        WriteComponentForTesting(
            component_dir_.GetPath(),
            CreateSerializedUrlParamFilterClassificationForTesting(
                {{"source.test", {"plzblock_src"}}},
                {{"dest.test", {"plzblock_dest"}}}, {DEFAULT_TAG}));
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(ClassificationsLoaderFeatureEnabledAndComponentInstalled,
                       LoaderUsesComponentClassifications) {
  // Since no feature classifications are provided, the expected
  // classifications should be the component classifications.
  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey("source.test"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock_src",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(DestinationKey("dest.test"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock_dest",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

// Feature enabled with just "should_filter"= unset/true/false, and the
// component is not installed.
class
    ClassificationsLoaderFeatureEnabledWithShouldFilterAndComponentNotInstalled
    : public ClassificationsLoaderBrowserTest,
      public testing::WithParamInterface<ShouldFilterState> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"should_filter", ShouldFilterStateToString(GetParam())}});
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_P(
    ClassificationsLoaderFeatureEnabledWithShouldFilterAndComponentNotInstalled,
    NeitherSourceProvidesClassifications) {
  // ClassificationLoader has no classifications since neither feature
  // classifications nor Component Updater classifications were provided.
  EXPECT_THAT(loader()->GetClassifications(), IsEmpty());
}

INSTANTIATE_TEST_CASE_P(
    /* no label */,
    ClassificationsLoaderFeatureEnabledWithShouldFilterAndComponentNotInstalled,
    ::testing::Values(ShouldFilterState::kUnset,
                      ShouldFilterState::kTrue,
                      ShouldFilterState::kFalse));

// Feature enabled with just "should_filter"= unset/true/false, and the
// component is installed.
class ClassificationsLoaderFeatureEnabledWithShouldFilterAndComponentInstalled
    : public ClassificationsLoaderBrowserTest,
      public testing::WithParamInterface<ShouldFilterState> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"should_filter", ShouldFilterStateToString(GetParam())}});
    // Force install the component.
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;

    component_updater::UrlParamClassificationComponentInstallerPolicy::
        WriteComponentForTesting(
            component_dir_.GetPath(),
            CreateSerializedUrlParamFilterClassificationForTesting(
                {{"source.test", {"plzblock_src"}}},
                {{"dest.test", {"plzblock_dest"}}}, {DEFAULT_TAG}));
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_P(
    ClassificationsLoaderFeatureEnabledWithShouldFilterAndComponentInstalled,
    LoaderUsesComponentClassifications) {
  // Since no feature classifications are provided, the expected
  // classifications should be the component classifications.
  ClassificationMap s = loader()->GetClassifications();
  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey("source.test"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock_src",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(DestinationKey("dest.test"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock_dest",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

INSTANTIATE_TEST_CASE_P(
    /* no label */,
    ClassificationsLoaderFeatureEnabledWithShouldFilterAndComponentInstalled,
    ::testing::Values(ShouldFilterState::kUnset,
                      ShouldFilterState::kTrue,
                      ShouldFilterState::kFalse));

// Feature enabled with just "classifications" param, and the
// component is not installed.
class
    ClassificationsLoaderFeatureEnabledWithClassificationsAndComponentNotInstalled
    : public ClassificationsLoaderBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"classifications",
          CreateBase64EncodedFilterParamClassificationForTesting(
              {{"feature-src.test", {"plzblock1"}}},
              {{"feature-dst.test", {"plzblock2"}}})}});
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(
    ClassificationsLoaderFeatureEnabledWithClassificationsAndComponentNotInstalled,
    LoaderUsesClassificationsFromFeature) {
  // ClassificationLoader uses the feature parameters
  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey("feature-src.test"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(DestinationKey("feature-dst.test"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

// Feature enabled with just "classifications" param, and the component is
// installed.
class
    ClassificationsLoaderFeatureEnabledWithClassificationsAndComponentInstalled
    : public ClassificationsLoaderBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"classifications",
          CreateBase64EncodedFilterParamClassificationForTesting(
              {{"feature-src.test", {"plzblock1"}}},
              {{"feature-dst.test", {"plzblock2"}}})}});
    // Force install the component.
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;

    component_updater::UrlParamClassificationComponentInstallerPolicy::
        WriteComponentForTesting(
            component_dir_.GetPath(),
            CreateSerializedUrlParamFilterClassificationForTesting(
                {{"source.test", {"plzblock_src"}}},
                {{"dest.test", {"plzblock_dest"}}}, {DEFAULT_TAG}));
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(
    ClassificationsLoaderFeatureEnabledWithClassificationsAndComponentInstalled,
    LoaderUsesFeatureClassifications) {
  // Since both feature and component classifications are provided, the feature
  // classifications take precedence.
  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey("feature-src.test"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(DestinationKey("feature-dst.test"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

// Feature enabled with "should_filter"= unset/true/false and a classifications
// param, and the component isn't installed.
class ClassificationsLoaderFeatureEnabledWithAllParamsAndComponentNotInstalled
    : public ClassificationsLoaderBrowserTest,
      public testing::WithParamInterface<ShouldFilterState> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"should_filter", ShouldFilterStateToString(GetParam())},
         {"classifications",
          CreateBase64EncodedFilterParamClassificationForTesting(
              {{"feature-src.test", {"plzblock1"}}},
              {{"feature-dst.test", {"plzblock2"}}})}});
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_P(
    ClassificationsLoaderFeatureEnabledWithAllParamsAndComponentNotInstalled,
    LoaderUsesFeatureClassifications) {
  // Since both feature and component classifications are provided, the feature
  // classifications take precedence.
  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey("feature-src.test"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(DestinationKey("feature-dst.test"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

INSTANTIATE_TEST_CASE_P(
    /* no label */,
    ClassificationsLoaderFeatureEnabledWithAllParamsAndComponentNotInstalled,
    ::testing::Values(ShouldFilterState::kUnset,
                      ShouldFilterState::kTrue,
                      ShouldFilterState::kFalse));

// Feature enabled with both the "should_filter" param unset/true/false and
// "classifications" param, and the component is also installed.
class ClassificationsLoaderFeatureEnabledWithAllParamsAndComponentInstalled
    : public ClassificationsLoaderBrowserTest,
      public testing::WithParamInterface<ShouldFilterState> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kIncognitoParamFilterEnabled,
        {{"should_filter", ShouldFilterStateToString(GetParam())},
         {"classifications",
          CreateBase64EncodedFilterParamClassificationForTesting(
              {{"feature-src.test", {"plzblock1"}}},
              {{"feature-dst.test", {"plzblock2"}}})}});
    // Force install the component.
    CHECK(component_dir_.CreateUniqueTempDir());
    base::ScopedAllowBlockingForTesting allow_blocking;

    component_updater::UrlParamClassificationComponentInstallerPolicy::
        WriteComponentForTesting(
            component_dir_.GetPath(),
            CreateSerializedUrlParamFilterClassificationForTesting(
                {{"source.test", {"plzblock_src"}}},
                {{"dest.test", {"plzblock_dest"}}}, {DEFAULT_TAG}));
    ClassificationsLoaderBrowserTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_P(
    ClassificationsLoaderFeatureEnabledWithAllParamsAndComponentInstalled,
    LoaderUsesFeatureClassifications) {
  // Since both feature and component classifications are provided, the feature
  // classifications take precedence.
  EXPECT_THAT(
      loader()->GetClassifications(),
      UnorderedElementsAre(
          Pair(SourceKey("feature-src.test"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock1",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL))))),
          Pair(DestinationKey("feature-dst.test"),
               UnorderedElementsAre(Pair(
                   FilterClassification::USE_CASE_UNKNOWN,
                   UnorderedElementsAre(Pair(
                       "plzblock2",
                       ClassificationExperimentStatus::NON_EXPERIMENTAL)))))));
}

INSTANTIATE_TEST_CASE_P(
    /* no label */,
    ClassificationsLoaderFeatureEnabledWithAllParamsAndComponentInstalled,
    ::testing::Values(ShouldFilterState::kUnset,
                      ShouldFilterState::kTrue,
                      ShouldFilterState::kFalse));

}  // namespace url_param_filter
