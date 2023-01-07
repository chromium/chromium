// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/voice_suggest_provider.h"

#include <memory>

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/omnibox/browser/fake_autocomplete_provider_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class VoiceSuggestProviderTest : public testing::Test {
 public:
  VoiceSuggestProviderTest() = default;
  VoiceSuggestProviderTest(const VoiceSuggestProviderTest&) = delete;
  VoiceSuggestProviderTest& operator=(const VoiceSuggestProviderTest&) = delete;

  void SetUp() override;
  void TearDown() override;

 protected:
  base::test::TaskEnvironment environment_;
  FakeAutocompleteProviderClient client_;
  scoped_refptr<VoiceSuggestProvider> provider_;
  std::unique_ptr<AutocompleteInput> input_;
};

void VoiceSuggestProviderTest::SetUp() {
  provider_ = base::MakeRefCounted<VoiceSuggestProvider>(&client_);
  input_ = std::make_unique<AutocompleteInput>(
      std::u16string(), metrics::OmniboxEventProto::OTHER,
      TestSchemeClassifier());
}

void VoiceSuggestProviderTest::TearDown() {
  provider_->Stop(true, true);
}

TEST_F(VoiceSuggestProviderTest, ServesNoSuggestionsByDefault) {
  provider_->Start(*input_, false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(VoiceSuggestProviderTest, ServesSuppliedVoiceSuggestions) {
  provider_->AddVoiceSuggestion(u"Alice", 0.6f);
  provider_->AddVoiceSuggestion(u"Bob", 0.5f);
  provider_->AddVoiceSuggestion(u"Carol", 0.4f);
  provider_->Start(*input_, false);

  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"Alice", provider_->matches()[0].contents);
  EXPECT_EQ(u"Bob", provider_->matches()[1].contents);
  EXPECT_EQ(u"Carol", provider_->matches()[2].contents);
}

TEST_F(VoiceSuggestProviderTest, ConfidenceScoreImpliesOrdering) {
  provider_->AddVoiceSuggestion(u"Carol", 0.4f);
  provider_->AddVoiceSuggestion(u"Bob", 0.5f);
  provider_->AddVoiceSuggestion(u"Alice", 0.6f);
  provider_->Start(*input_, false);

  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"Alice", provider_->matches()[0].contents);
  EXPECT_EQ(u"Bob", provider_->matches()[1].contents);
  EXPECT_EQ(u"Carol", provider_->matches()[2].contents);
}

TEST_F(VoiceSuggestProviderTest,
       VoiceSuggestionsAreNotReusedInSubsequentRequests) {
  provider_->AddVoiceSuggestion(u"Alice", 0.6f);
  provider_->AddVoiceSuggestion(u"Bob", 0.5f);
  provider_->AddVoiceSuggestion(u"Carol", 0.4f);
  provider_->Start(*input_, false);
  provider_->Stop(true, false);
  provider_->Start(*input_, false);

  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(VoiceSuggestProviderTest, ClearCachePurgesAvailableVoiceSuggestions) {
  provider_->AddVoiceSuggestion(u"Alice", 0.6f);
  provider_->AddVoiceSuggestion(u"Bob", 0.5f);
  provider_->AddVoiceSuggestion(u"Carol", 0.4f);
  provider_->ClearCache();
  provider_->Start(*input_, false);

  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(VoiceSuggestProviderTest, MatchesWithSameScoresAreNotDropped) {
  provider_->AddVoiceSuggestion(u"Alice", 0.6f);
  provider_->AddVoiceSuggestion(u"Bob", 0.6f);
  provider_->AddVoiceSuggestion(u"Carol", 0.6f);
  provider_->Start(*input_, false);

  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"Alice", provider_->matches()[0].contents);
  EXPECT_EQ(u"Bob", provider_->matches()[1].contents);
  EXPECT_EQ(u"Carol", provider_->matches()[2].contents);
}

TEST_F(VoiceSuggestProviderTest, DuplicateMatchesAreMerged) {
  provider_->AddVoiceSuggestion(u"Alice", 0.6f);
  provider_->AddVoiceSuggestion(u"Bob", 0.5f);
  provider_->AddVoiceSuggestion(u"Bob", 0.4f);
  provider_->Start(*input_, false);

  ASSERT_EQ(2U, provider_->matches().size());
  EXPECT_EQ(u"Alice", provider_->matches()[0].contents);
  EXPECT_EQ(u"Bob", provider_->matches()[1].contents);
}

TEST_F(VoiceSuggestProviderTest, HighConfidenceScoreDropsAlternatives) {
  provider_->AddVoiceSuggestion(u"Alice", 0.9f);
  provider_->AddVoiceSuggestion(u"Bob", 0.5f);
  provider_->AddVoiceSuggestion(u"Carol", 0.4f);
  provider_->Start(*input_, false);

  ASSERT_EQ(1U, provider_->matches().size());
  EXPECT_EQ(u"Alice", provider_->matches()[0].contents);
}

TEST_F(VoiceSuggestProviderTest, LowConfidenceScoresAreRejected) {
  provider_->AddVoiceSuggestion(u"Alice", 0.35f);
  provider_->AddVoiceSuggestion(u"Bob", 0.25f);
  provider_->AddVoiceSuggestion(u"Carol", 0.2f);
  provider_->Start(*input_, false);

  ASSERT_EQ(1U, provider_->matches().size());
  EXPECT_EQ(u"Alice", provider_->matches()[0].contents);
}

TEST_F(VoiceSuggestProviderTest, VoiceSuggestionResultsCanBeLimited) {
  provider_->AddVoiceSuggestion(u"Alice", 0.75f);
  provider_->AddVoiceSuggestion(u"Bob", 0.65f);
  provider_->AddVoiceSuggestion(u"Carol", 0.55f);
  provider_->AddVoiceSuggestion(u"Dave", 0.45f);
  provider_->AddVoiceSuggestion(u"Eve", 0.35f);
  provider_->Start(*input_, false);

  ASSERT_EQ(3U, provider_->matches().size());
  EXPECT_EQ(u"Alice", provider_->matches()[0].contents);
  EXPECT_EQ(u"Bob", provider_->matches()[1].contents);
  EXPECT_EQ(u"Carol", provider_->matches()[2].contents);
}
