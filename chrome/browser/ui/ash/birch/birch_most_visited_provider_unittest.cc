// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_most_visited_provider.h"

#include "ash/birch/birch_model.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace {

class TestHistoryService : public history::HistoryService {
 public:
  TestHistoryService() = default;
  TestHistoryService(const TestHistoryService&) = delete;
  TestHistoryService& operator=(const TestHistoryService&) = delete;
  ~TestHistoryService() override = default;

  // history::HistoryService:
  base::CancelableTaskTracker::TaskId QueryMostVisitedURLs(
      int result_count,
      QueryMostVisitedURLsCallback callback,
      base::CancelableTaskTracker* tracker) override {
    did_query_most_visited_urls_ = true;
    return base::CancelableTaskTracker::TaskId();
  }

  bool did_query_most_visited_urls_ = false;
};

class TestFaviconService : public favicon::MockFaviconService {
 public:
  TestFaviconService() = default;
  TestFaviconService(const TestFaviconService&) = delete;
  TestFaviconService& operator=(const TestFaviconService&) = delete;
  ~TestFaviconService() override = default;

  // favicon::FaviconService:
  base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker) override {
    did_get_favicon_image_for_page_url_ = true;
    page_url_ = page_url;
    return base::CancelableTaskTracker::TaskId();
  }

  bool did_get_favicon_image_for_page_url_ = false;
  GURL page_url_;
};

// BrowserWithTestWindowTest provides a Profile and ash::Shell (which provides
// a BirchModel) needed by the test.
class BirchMostVisitedProviderTest : public BrowserWithTestWindowTest {
 public:
  BirchMostVisitedProviderTest() {
    switches::SetIgnoreForestSecretKeyForTest(true);
    feature_list_.InitAndEnableFeature(features::kForestFeature);
  }
  ~BirchMostVisitedProviderTest() override {
    switches::SetIgnoreForestSecretKeyForTest(false);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BirchMostVisitedProviderTest, RequestBirchDataFetch) {
  BirchMostVisitedProvider provider(profile());

  TestHistoryService history_service;
  provider.set_history_service_for_test(&history_service);
  TestFaviconService favicon_service;
  provider.set_favicon_service_for_test(&favicon_service);

  // Requesting a data fetch should query most visited URLs.
  provider.RequestBirchDataFetch();
  EXPECT_TRUE(history_service.did_query_most_visited_urls_);

  // Simulate a most-visited URL.
  history::MostVisitedURLList urls;
  history::MostVisitedURL url;
  url.title = u"title";
  url.url = GURL("http://example.com/");
  urls.push_back(url);

  // Once the most visited URLs are fetched the favicon database is queried.
  provider.OnGotMostVisitedURLs(urls);
  EXPECT_TRUE(favicon_service.did_get_favicon_image_for_page_url_);
  EXPECT_EQ(favicon_service.page_url_, GURL("http://example.com/"));

  // Simulate a favicon image.
  favicon_base::FaviconImageResult image_result;
  image_result.image = gfx::test::CreateImage(16);

  // Once the favicon is fetched the birch model is populated.
  provider.OnGotFaviconImage(url.title, url.url, image_result);
  auto* birch_model = Shell::Get()->birch_model();
  EXPECT_EQ(birch_model->GetMostVisitedItemsForTest().size(), 1u);

  // Reset the birch model most visited items.
  birch_model->SetMostVisitedItems({});

  // Simulate a fetch for the same most-visited URL. The data should come out of
  // cache and the favicon load is not required.
  provider.RequestBirchDataFetch();
  provider.OnGotMostVisitedURLs(urls);

  // The birch model is populated without a favicon load.
  EXPECT_EQ(birch_model->GetMostVisitedItemsForTest().size(), 1u);
}

}  // namespace
}  // namespace ash
