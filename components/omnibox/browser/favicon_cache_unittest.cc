// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/favicon_cache.h"

#include "base/bind.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/favicon_size.h"

using testing::_;
using testing::DoAll;
using testing::Return;

namespace {

favicon_base::FaviconImageResult GetDummyFaviconResult() {
  favicon_base::FaviconImageResult result;

  result.icon_url = GURL("http://example.com/favicon.ico");

  SkBitmap bitmap;
  bitmap.allocN32Pixels(gfx::kFaviconSize, gfx::kFaviconSize);
  bitmap.eraseColor(SK_ColorBLUE);
  result.image = gfx::Image::CreateFrom1xBitmap(bitmap);

  return result;
}

void VerifyFetchedFaviconAndCount(int* count, const gfx::Image& favicon) {
  DCHECK(count);
  EXPECT_FALSE(favicon.IsEmpty());
  ++(*count);
}

void VerifyFetchedFavicon(const gfx::Image& favicon) {
  EXPECT_FALSE(favicon.IsEmpty());
}

void Fail(const gfx::Image& favicon) {
  FAIL() << "This asynchronous callback should never have been called.";
}

}  // namespace

class FaviconCacheTest : public testing::Test {
 protected:
  const GURL kUrlA = GURL("http://www.a.com/");
  const GURL kUrlB = GURL("http://www.b.com/");
  const GURL kIconUrl = GURL("http://a.com/favicon.ico");

  FaviconCacheTest()
      : cache_(&favicon_service_, nullptr /* history_service */) {}

  testing::NiceMock<favicon::MockFaviconService> favicon_service_;

  void ExpectFaviconServiceForPageUrlCalls(int a_site_calls, int b_site_calls) {
    if (a_site_calls > 0) {
      EXPECT_CALL(
          favicon_service_,
          GetFaviconImageForPageURL(kUrlA, _ /* callback */, _ /* tracker */))
          .Times(a_site_calls)
          .WillRepeatedly(
              [&](auto, favicon_base::FaviconImageCallback callback, auto) {
                favicon_service_a_site_response_ = std::move(callback);
                return base::CancelableTaskTracker::kBadTaskId;
              });
    }

    if (b_site_calls > 0) {
      EXPECT_CALL(
          favicon_service_,
          GetFaviconImageForPageURL(kUrlB, _ /* callback */, _ /* tracker */))
          .Times(b_site_calls)
          .WillRepeatedly(
              [&](auto, favicon_base::FaviconImageCallback callback, auto) {
                favicon_service_b_site_response_ = std::move(callback);
                return base::CancelableTaskTracker::kBadTaskId;
              });
    }
  }

  void ExpectFaviconServiceForIconUrlCalls(int calls) {
    EXPECT_CALL(favicon_service_,
                GetFaviconImage(kIconUrl, _ /* callback */, _ /* tracker */))
        .Times(calls);
  }

  favicon_base::FaviconImageCallback favicon_service_a_site_response_;
  favicon_base::FaviconImageCallback favicon_service_b_site_response_;

  FaviconCache cache_;
};

TEST_F(FaviconCacheTest, Basic) {
  ExpectFaviconServiceForPageUrlCalls(1, 0);
  ExpectFaviconServiceForIconUrlCalls(0);

  int response_count = 0;
  gfx::Image result = cache_.GetFaviconForPageUrl(
      kUrlA, base::BindOnce(&VerifyFetchedFaviconAndCount, &response_count));

  // Expect the synchronous result to be empty.
  EXPECT_TRUE(result.IsEmpty());

  std::move(favicon_service_a_site_response_).Run(GetDummyFaviconResult());

  // Re-request the same favicon and expect a non-empty result now that the
  // cache is populated. The above EXPECT_CALL will also verify that the
  // backing FaviconService is not hit again.
  result = cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail));

  EXPECT_FALSE(result.IsEmpty());
  EXPECT_EQ(1, response_count);
}

TEST_F(FaviconCacheTest, GetFaviconForIconUrl) {
  // Verify that the service receives a request by the icon URL.
  ExpectFaviconServiceForPageUrlCalls(0, 0);
  ExpectFaviconServiceForIconUrlCalls(1);

  // Since the other tests are comprehensive, we don't simulate or verify the
  // actual result.
  gfx::Image result =
      cache_.GetFaviconForIconUrl(kIconUrl, base::BindOnce(&Fail));
  EXPECT_TRUE(result.IsEmpty());
}

TEST_F(FaviconCacheTest, MultipleRequestsAreCoalesced) {
  ExpectFaviconServiceForPageUrlCalls(1, 0);

  int response_count = 0;
  for (int i = 0; i < 10; ++i) {
    cache_.GetFaviconForPageUrl(
        kUrlA, base::BindOnce(&VerifyFetchedFaviconAndCount, &response_count));
  }

  std::move(favicon_service_a_site_response_).Run(GetDummyFaviconResult());

  EXPECT_EQ(10, response_count);
}

TEST_F(FaviconCacheTest, SeparateOriginsAreCachedSeparately) {
  ExpectFaviconServiceForPageUrlCalls(1, 1);

  int a_site_response_count = 0;
  int b_site_response_count = 0;

  gfx::Image a_site_return = cache_.GetFaviconForPageUrl(
      kUrlA,
      base::BindOnce(&VerifyFetchedFaviconAndCount, &a_site_response_count));
  gfx::Image b_site_return = cache_.GetFaviconForPageUrl(
      kUrlB,
      base::BindOnce(&VerifyFetchedFaviconAndCount, &b_site_response_count));

  EXPECT_TRUE(a_site_return.IsEmpty());
  EXPECT_TRUE(b_site_return.IsEmpty());
  EXPECT_EQ(0, a_site_response_count);
  EXPECT_EQ(0, b_site_response_count);

  std::move(favicon_service_b_site_response_).Run(GetDummyFaviconResult());

  EXPECT_EQ(0, a_site_response_count);
  EXPECT_EQ(1, b_site_response_count);

  a_site_return = cache_.GetFaviconForPageUrl(
      kUrlA,
      base::BindOnce(&VerifyFetchedFaviconAndCount, &a_site_response_count));
  b_site_return = cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail));

  EXPECT_TRUE(a_site_return.IsEmpty());
  EXPECT_FALSE(b_site_return.IsEmpty());
  EXPECT_EQ(0, a_site_response_count);
  EXPECT_EQ(1, b_site_response_count);

  std::move(favicon_service_a_site_response_).Run(GetDummyFaviconResult());

  EXPECT_EQ(2, a_site_response_count);
  EXPECT_EQ(1, b_site_response_count);

  a_site_return = cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail));
  b_site_return = cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail));

  EXPECT_FALSE(a_site_return.IsEmpty());
  EXPECT_FALSE(b_site_return.IsEmpty());
}

TEST_F(FaviconCacheTest, ClearIconsWithHistoryDeletions) {
  ExpectFaviconServiceForPageUrlCalls(3, 2);

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&VerifyFetchedFavicon))
          .IsEmpty());
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&VerifyFetchedFavicon))
          .IsEmpty());

  std::move(favicon_service_a_site_response_).Run(GetDummyFaviconResult());
  std::move(favicon_service_b_site_response_).Run(GetDummyFaviconResult());

  EXPECT_FALSE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
  EXPECT_FALSE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail)).IsEmpty());

  // Delete just the entry for kUrlA.
  history::URLRows a_rows = {history::URLRow(kUrlA)};
  cache_.OnURLsDeleted(
      nullptr /* history_service */,
      history::DeletionInfo::ForUrls(a_rows, {} /* favicon_urls */));

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&VerifyFetchedFavicon))
          .IsEmpty());
  EXPECT_FALSE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail)).IsEmpty());

  // Restore the cache entry for kUrlA.
  std::move(favicon_service_a_site_response_).Run(GetDummyFaviconResult());

  // Delete all history.
  cache_.OnURLsDeleted(nullptr /* history_service */,
                       history::DeletionInfo::ForAllHistory());

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&VerifyFetchedFavicon))
          .IsEmpty());
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&VerifyFetchedFavicon))
          .IsEmpty());
}

TEST_F(FaviconCacheTest, CacheNullFavicons) {
  ExpectFaviconServiceForPageUrlCalls(1, 0);

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
  std::move(favicon_service_a_site_response_)
      .Run(favicon_base::FaviconImageResult());

  // The mock FaviconService's EXPECT_CALL verifies that we do not make another
  // call to FaviconService.
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
}

TEST_F(FaviconCacheTest, ExpireNullFaviconsByHistory) {
  ExpectFaviconServiceForPageUrlCalls(2, 0);

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
  std::move(favicon_service_a_site_response_)
      .Run(favicon_base::FaviconImageResult());

  cache_.OnURLVisited(nullptr /* history_service */, ui::PAGE_TRANSITION_LINK,
                      history::URLRow(kUrlA), history::RedirectList(),
                      base::Time::Now());

  // Now the empty favicon should have been expired and we expect our second
  // call to the mock underlying FaviconService.
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&VerifyFetchedFavicon))
          .IsEmpty());
  std::move(favicon_service_a_site_response_).Run(GetDummyFaviconResult());
  EXPECT_FALSE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
}

TEST_F(FaviconCacheTest, ObserveFaviconsChanged) {
  ExpectFaviconServiceForPageUrlCalls(2, 1);

  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail)).IsEmpty());

  // Simulate responses to both requests.
  std::move(favicon_service_a_site_response_)
      .Run(favicon_base::FaviconImageResult());
  std::move(favicon_service_b_site_response_)
      .Run(favicon_base::FaviconImageResult());

  cache_.OnFaviconsChanged({kUrlA}, GURL());

  // Request the two favicons again.
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlA, base::BindOnce(&Fail)).IsEmpty());
  EXPECT_TRUE(
      cache_.GetFaviconForPageUrl(kUrlB, base::BindOnce(&Fail)).IsEmpty());

  // Our call to |ExpectFaviconServiceForPageUrlCalls(expected A calls, expected
  // B calls)| above should verify that we re-request the icon for kUrlA only
  // (because the null result has been invalidated by OnFaviconsChanged).
}
