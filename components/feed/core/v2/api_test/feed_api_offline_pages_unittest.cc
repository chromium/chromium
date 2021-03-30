// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "components/feed/core/v2/api_test/feed_api_test.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feed {
namespace test {
namespace {

TEST_F(FeedApiTest, ProvidesPrefetchSuggestionsWhenModelLoaded) {
  // Setup by triggering a model load.
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  // Because we loaded from the network,
  // PrefetchService::NewSuggestionsAvailable() should have been called.
  EXPECT_EQ(1, prefetch_service_.NewSuggestionsAvailableCallCount());

  CallbackReceiver<std::vector<offline_pages::PrefetchSuggestion>> callback;
  prefetch_service_.suggestions_provider()->GetCurrentArticleSuggestions(
      callback.Bind());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(callback.GetResult());
  const std::vector<offline_pages::PrefetchSuggestion>& suggestions =
      callback.GetResult().value();

  ASSERT_EQ(2UL, suggestions.size());
  EXPECT_EQ("http://content0/", suggestions[0].article_url);
  EXPECT_EQ("title0", suggestions[0].article_title);
  EXPECT_EQ("publisher0", suggestions[0].article_attribution);
  EXPECT_EQ("snippet0", suggestions[0].article_snippet);
  EXPECT_EQ("http://image0/", suggestions[0].thumbnail_url);
  EXPECT_EQ("http://favicon0/", suggestions[0].favicon_url);

  EXPECT_EQ("http://content1/", suggestions[1].article_url);
}

TEST_F(FeedApiTest, ProvidesPrefetchSuggestionsWhenModelNotLoaded) {
  store_->OverwriteStream(kForYouStream, MakeTypicalInitialModelState(),
                          base::DoNothing());

  CallbackReceiver<std::vector<offline_pages::PrefetchSuggestion>> callback;
  prefetch_service_.suggestions_provider()->GetCurrentArticleSuggestions(
      callback.Bind());
  WaitForIdleTaskQueue();

  ASSERT_FALSE(stream_->GetModel(kForYouStream));
  ASSERT_TRUE(callback.GetResult());
  const std::vector<offline_pages::PrefetchSuggestion>& suggestions =
      callback.GetResult().value();

  ASSERT_EQ(2UL, suggestions.size());
  EXPECT_EQ("http://content0/", suggestions[0].article_url);
  EXPECT_EQ("http://content1/", suggestions[1].article_url);
  EXPECT_EQ(0, prefetch_service_.NewSuggestionsAvailableCallCount());
}

TEST_F(FeedApiTest, ScrubsUrlsInProvidedPrefetchSuggestions) {
  {
    auto initial_state = MakeTypicalInitialModelState();
    initial_state->content[0].mutable_prefetch_metadata(0)->set_uri(
        "?notavalidurl?");
    initial_state->content[0].mutable_prefetch_metadata(0)->set_image_url(
        "?asdf?");
    initial_state->content[0].mutable_prefetch_metadata(0)->set_favicon_url(
        "?hi?");
    initial_state->content[0].mutable_prefetch_metadata(0)->clear_uri();
    store_->OverwriteStream(kForYouStream, std::move(initial_state),
                            base::DoNothing());
  }

  CallbackReceiver<std::vector<offline_pages::PrefetchSuggestion>> callback;
  prefetch_service_.suggestions_provider()->GetCurrentArticleSuggestions(
      callback.Bind());
  WaitForIdleTaskQueue();

  ASSERT_TRUE(callback.GetResult());
  const std::vector<offline_pages::PrefetchSuggestion>& suggestions =
      callback.GetResult().value();

  ASSERT_EQ(2UL, suggestions.size());
  EXPECT_EQ("", suggestions[0].article_url.possibly_invalid_spec());
  EXPECT_EQ("", suggestions[0].thumbnail_url.possibly_invalid_spec());
  EXPECT_EQ("", suggestions[0].favicon_url.possibly_invalid_spec());
}

TEST_F(FeedApiTest, OfflineBadgesArePopulatedInitially) {
  // Add two offline pages. We exclude tab-bound pages, so only the first is
  // used.
  offline_page_model_.AddTestPage(GURL("http://content0/"));
  offline_page_model_.AddTestPage(GURL("http://content1/"));
  offline_page_model_.items()[1].client_id.name_space =
      offline_pages::kLastNNamespace;
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ((std::map<std::string, std::string>(
                {{"app/badge0", SerializedOfflineBadgeContent()}})),
            surface.GetDataStoreEntries());
}

TEST_F(FeedApiTest, OfflineBadgesArePopulatedOnNewOfflineItemAdded) {
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ((std::map<std::string, std::string>({})),
            surface.GetDataStoreEntries());

  // Add an offline page.
  offline_page_model_.AddTestPage(GURL("http://content1/"));
  offline_page_model_.CallObserverOfflinePageAdded(
      offline_page_model_.items()[0]);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));

  EXPECT_EQ((std::map<std::string, std::string>(
                {{"app/badge1", SerializedOfflineBadgeContent()}})),
            surface.GetDataStoreEntries());
}

TEST_F(FeedApiTest, OfflineBadgesAreRemovedWhenOfflineItemRemoved) {
  offline_page_model_.AddTestPage(GURL("http://content0/"));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  ASSERT_EQ((std::map<std::string, std::string>(
                {{"app/badge0", SerializedOfflineBadgeContent()}})),
            surface.GetDataStoreEntries());

  // Remove the offline page.
  offline_page_model_.CallObserverOfflinePageDeleted(
      offline_page_model_.items()[0]);
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));

  EXPECT_EQ((std::map<std::string, std::string>()),
            surface.GetDataStoreEntries());
}

TEST_F(FeedApiTest, OfflineBadgesAreProvidedToNewSurfaces) {
  offline_page_model_.AddTestPage(GURL("http://content0/"));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  TestForYouSurface surface2(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ((std::map<std::string, std::string>(
                {{"app/badge0", SerializedOfflineBadgeContent()}})),
            surface2.GetDataStoreEntries());
}

TEST_F(FeedApiTest, OfflineBadgesAreRemovedWhenModelIsUnloaded) {
  offline_page_model_.AddTestPage(GURL("http://content0/"));
  response_translator_.InjectResponse(MakeTypicalInitialModelState());
  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  stream_->UnloadModel(surface.GetStreamType());

  // Offline badge no longer present.
  EXPECT_EQ((std::map<std::string, std::string>()),
            surface.GetDataStoreEntries());
}

TEST_F(FeedApiTest, MultipleOfflineBadgesWithSameUrl) {
  {
    std::unique_ptr<StreamModelUpdateRequest> state =
        MakeTypicalInitialModelState();
    const feedwire::PrefetchMetadata& prefetch_metadata1 =
        state->content[0].prefetch_metadata(0);
    feedwire::PrefetchMetadata& prefetch_metadata2 =
        *state->content[0].add_prefetch_metadata();
    prefetch_metadata2 = prefetch_metadata1;
    prefetch_metadata2.set_badge_id("app/badge0b");
    response_translator_.InjectResponse(std::move(state));
  }
  offline_page_model_.AddTestPage(GURL("http://content0/"));

  TestForYouSurface surface(stream_.get());
  WaitForIdleTaskQueue();

  EXPECT_EQ((std::map<std::string, std::string>(
                {{"app/badge0", SerializedOfflineBadgeContent()},
                 {"app/badge0b", SerializedOfflineBadgeContent()}})),
            surface.GetDataStoreEntries());
}

}  // namespace
}  // namespace test
}  // namespace feed
