// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/client_side_detection_feature_cache.h"

#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::RenderFrameHost;
using content::RenderFrameHostTester;

namespace safe_browsing {

typedef content::RenderViewHostTestHarness ClientSideDetectionFeatureCacheTest;

TEST_F(ClientSideDetectionFeatureCacheTest, MaintainVerdictMapCap) {
  content::WebContents* content = web_contents();
  ClientSideDetectionFeatureCache::CreateForWebContents(content);
  ClientSideDetectionFeatureCache* cache_ =
      ClientSideDetectionFeatureCache::FromWebContents(content);
  std::string string_url = "https://www.testtest1234.com/";
  GURL url(string_url);
  std::unique_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  verdict->set_dom_model_version(100);
  cache_->InsertVerdict(url, std::move(verdict));

  for (size_t count = 0; count < cache_->GetMaxMapCapacity() - 1; count++) {
    cache_->InsertVerdict(GURL(string_url + base::NumberToString(count)),
                          std::make_unique<ClientPhishingRequest>());
  }

  // This should equal to the first verdict we inserted into the cache.
  EXPECT_EQ(cache_->GetVerdictForURL(url)->dom_model_version(), 100);

  cache_->InsertVerdict(GURL("https://www.testtest.com"),
                        std::make_unique<ClientPhishingRequest>());

  // A blank verdict has been inserted after the cap is reached, so the first
  // verdict should no longer exist.
  EXPECT_EQ(cache_->GetVerdictForURL(url), nullptr);
}

TEST_F(ClientSideDetectionFeatureCacheTest, VerdictEntriesSize) {
  content::WebContents* content = web_contents();
  ClientSideDetectionFeatureCache::CreateForWebContents(content);
  ClientSideDetectionFeatureCache* cache_ =
      ClientSideDetectionFeatureCache::FromWebContents(content);

  EXPECT_EQ(cache_->GetTotalVerdictEntriesSize(), 0);

  std::string string_url = "https://www.testtest1234.com/";

  for (size_t count = 0; count < cache_->GetMaxMapCapacity(); count++) {
    cache_->InsertVerdict(GURL(string_url + base::NumberToString(count)),
                          std::make_unique<ClientPhishingRequest>());
  }

  long total_entries_size_with_empty_verdict =
      cache_->GetTotalVerdictEntriesSize();

  EXPECT_EQ(total_entries_size_with_empty_verdict, 0);

  // Insert another empty verdict to the cache when the cache is full.
  cache_->InsertVerdict(GURL(string_url + "0000"),
                        std::make_unique<ClientPhishingRequest>());

  EXPECT_EQ(cache_->GetTotalVerdictEntriesSize(),
            total_entries_size_with_empty_verdict);

  GURL url(string_url);
  std::unique_ptr<ClientPhishingRequest> verdict(new ClientPhishingRequest);
  verdict->set_is_phishing(true);
  verdict->set_dom_model_version(100);

  cache_->InsertVerdict(url, std::move(verdict));

  EXPECT_NE(cache_->GetTotalVerdictEntriesSize(),
            total_entries_size_with_empty_verdict);
}

}  // namespace safe_browsing
