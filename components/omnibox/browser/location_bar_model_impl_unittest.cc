// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/omnibox/browser/location_bar_model_delegate.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/common/omnibox_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class FakeLocationBarModelDelegate : public LocationBarModelDelegate {
 public:
  void SetURL(const GURL& url) { url_ = url; }
  void SetShouldPreventElision(bool should_prevent_elision) {
    should_prevent_elision_ = should_prevent_elision;
  }
  void SetSecurityLevel(security_state::SecurityLevel level) {
    security_level_ = level;
  }
  void SetVisibleSecurityStateConnectionInfoUninitialized() {
    connection_info_initialized_ = false;
  }

  // LocationBarModelDelegate:
  base::string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const base::string16& formatted_url) const override {
    return formatted_url + base::ASCIIToUTF16("/TestSuffix");
  }

  bool GetURL(GURL* url) const override {
    *url = url_;
    return true;
  }

  bool ShouldPreventElision() const override { return should_prevent_elision_; }

  security_state::SecurityLevel GetSecurityLevel() const override {
    return security_level_;
  }

  std::unique_ptr<security_state::VisibleSecurityState>
  GetVisibleSecurityState() const override {
    std::unique_ptr<security_state::VisibleSecurityState> state =
        std::make_unique<security_state::VisibleSecurityState>();
    state->connection_info_initialized = connection_info_initialized_;
    return state;
  }

  bool IsInstantNTP() const override { return false; }

  bool IsNewTabPage(const GURL& url) const override { return false; }

  bool IsHomePage(const GURL& url) const override { return false; }

  AutocompleteClassifier* GetAutocompleteClassifier() override {
    return omnibox_client_.GetAutocompleteClassifier();
  }

  TemplateURLService* GetTemplateURLService() override {
    return omnibox_client_.GetTemplateURLService();
  }

 private:
  GURL url_;
  security_state::SecurityLevel security_level_;
  TestOmniboxClient omnibox_client_;
  bool should_prevent_elision_ = false;
  bool connection_info_initialized_ = true;
};

class LocationBarModelImplTest : public testing::Test {
 protected:
  const GURL kValidSearchResultsPage =
      GURL("https://www.google.com/search?q=foo+query");

  LocationBarModelImplTest() : model_(&delegate_, 1024) {}

  FakeLocationBarModelDelegate* delegate() { return &delegate_; }

  LocationBarModelImpl* model() { return &model_; }

 private:
  base::test::TaskEnvironment task_environment_;
  FakeLocationBarModelDelegate delegate_;
  LocationBarModelImpl model_;
};

TEST_F(LocationBarModelImplTest,
       DisplayUrlAppliesFormattedStringWithEquivalentMeaning) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({omnibox::kHideSteadyStateUrlScheme,
                                 omnibox::kHideSteadyStateUrlTrivialSubdomains},
                                {});

  delegate()->SetURL(GURL("http://www.google.com/"));

  // Verify that both the full formatted URL and the display URL add the test
  // suffix.
  EXPECT_EQ(base::ASCIIToUTF16("www.google.com/TestSuffix"),
            model()->GetFormattedFullURL());
  EXPECT_EQ(base::ASCIIToUTF16("google.com/TestSuffix"),
            model()->GetURLForDisplay());
}

// TODO(https://crbug.com/1010418): Fix flakes on linux_chromium_asan_rel_ng and
// re-enable this test.
#if defined(OS_LINUX)
#define MAYBE_PreventElisionWorks DISABLED_PreventElisionWorks
#else
#define MAYBE_PreventElisionWorks PreventElisionWorks
#endif
TEST_F(LocationBarModelImplTest, MAYBE_PreventElisionWorks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {omnibox::kHideSteadyStateUrlScheme,
       omnibox::kHideSteadyStateUrlTrivialSubdomains, omnibox::kQueryInOmnibox},
      {});

  delegate()->SetShouldPreventElision(true);
  delegate()->SetURL(GURL("https://www.google.com/search?q=foo+query+unelide"));

  EXPECT_EQ(base::ASCIIToUTF16(
                "https://www.google.com/search?q=foo+query+unelide/TestSuffix"),
            model()->GetURLForDisplay());

  // Verify that query in omnibox is turned off.
  delegate()->SetSecurityLevel(security_state::SecurityLevel::SECURE);
  EXPECT_FALSE(model()->GetDisplaySearchTerms(nullptr));
}

TEST_F(LocationBarModelImplTest, QueryInOmniboxFeatureFlagWorks) {
  delegate()->SetURL(kValidSearchResultsPage);
  delegate()->SetSecurityLevel(security_state::SecurityLevel::SECURE);

  EXPECT_FALSE(model()->GetDisplaySearchTerms(nullptr));

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  EXPECT_TRUE(model()->GetDisplaySearchTerms(nullptr));
}

TEST_F(LocationBarModelImplTest, QueryInOmniboxSecurityLevel) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  delegate()->SetURL(kValidSearchResultsPage);

  delegate()->SetSecurityLevel(security_state::SecurityLevel::SECURE);
  EXPECT_TRUE(model()->GetDisplaySearchTerms(nullptr));

  delegate()->SetSecurityLevel(security_state::SecurityLevel::EV_SECURE);
  EXPECT_TRUE(model()->GetDisplaySearchTerms(nullptr));

  // Insecure levels should not be allowed to display search terms.
  delegate()->SetSecurityLevel(security_state::SecurityLevel::NONE);
  EXPECT_FALSE(model()->GetDisplaySearchTerms(nullptr));

  delegate()->SetSecurityLevel(security_state::SecurityLevel::DANGEROUS);
  EXPECT_FALSE(model()->GetDisplaySearchTerms(nullptr));

  // But ignore the level if the connection info has not been initialized.
  delegate()->SetVisibleSecurityStateConnectionInfoUninitialized();
  delegate()->SetSecurityLevel(security_state::SecurityLevel::NONE);
  EXPECT_TRUE(model()->GetDisplaySearchTerms(nullptr));
}

TEST_F(LocationBarModelImplTest,
       QueryInOmniboxDefaultSearchProviderWithAndWithoutQuery) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);
  delegate()->SetSecurityLevel(security_state::SecurityLevel::SECURE);

  delegate()->SetURL(kValidSearchResultsPage);
  base::string16 result;
  EXPECT_TRUE(model()->GetDisplaySearchTerms(&result));
  EXPECT_EQ(base::ASCIIToUTF16("foo query"), result);

  const GURL kDefaultSearchProviderURLWithNoQuery(
      "https://www.google.com/maps");
  result.clear();
  delegate()->SetURL(kDefaultSearchProviderURLWithNoQuery);
  EXPECT_FALSE(model()->GetDisplaySearchTerms(&result));
  EXPECT_EQ(base::string16(), result);
}

TEST_F(LocationBarModelImplTest, QueryInOmniboxNonDefaultSearchProvider) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  const GURL kNonDefaultSearchProvider(
      "https://search.yahoo.com/search?ei=UTF-8&fr=crmas&p=foo+query");
  delegate()->SetURL(kNonDefaultSearchProvider);
  delegate()->SetSecurityLevel(security_state::SecurityLevel::SECURE);

  base::string16 result;
  EXPECT_FALSE(model()->GetDisplaySearchTerms(&result));
  EXPECT_EQ(base::string16(), result);
}

TEST_F(LocationBarModelImplTest, QueryInOmniboxLookalikeURL) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(omnibox::kQueryInOmnibox);

  delegate()->SetSecurityLevel(security_state::SecurityLevel::SECURE);

  const GURL kLookalikeURLQuery(
      "https://www.google.com/search?q=lookalike.com");
  delegate()->SetURL(kLookalikeURLQuery);

  base::string16 result;
  EXPECT_FALSE(model()->GetDisplaySearchTerms(&result));
  EXPECT_EQ(base::string16(), result);
}

}  // namespace
