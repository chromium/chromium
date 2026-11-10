// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/client_side_detection/client_side_detection_feature_cache_base.h"

#include <memory>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

class ClientSideDetectionFeatureCacheBaseTest : public testing::Test {
 protected:
  void SetUp() override {
    cache_ = std::make_unique<ClientSideDetectionFeatureCacheBase>();
  }

  std::unique_ptr<ClientSideDetectionFeatureCacheBase> cache_;
};

TEST_F(ClientSideDetectionFeatureCacheBaseTest, MaintainVerdictMapCap) {
  std::string string_url = "https://www.testtest1234.com/";
  GURL url(string_url);
  auto verdict = std::make_unique<ClientPhishingRequest>();
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

TEST_F(ClientSideDetectionFeatureCacheBaseTest, VerdictEntriesSize) {
  EXPECT_EQ(cache_->GetTotalVerdictEntriesSize(), 0L);

  std::string string_url = "https://www.testtest1234.com/";

  for (size_t count = 0; count < cache_->GetMaxMapCapacity(); count++) {
    cache_->InsertVerdict(GURL(string_url + base::NumberToString(count)),
                          std::make_unique<ClientPhishingRequest>());
  }

  long total_entries_size_with_empty_verdict =
      cache_->GetTotalVerdictEntriesSize();

  EXPECT_EQ(total_entries_size_with_empty_verdict, 0L);

  // Insert another empty verdict to the cache when the cache is full.
  cache_->InsertVerdict(GURL(string_url + "0000"),
                        std::make_unique<ClientPhishingRequest>());

  EXPECT_EQ(cache_->GetTotalVerdictEntriesSize(),
            total_entries_size_with_empty_verdict);

  GURL url(string_url);
  auto verdict = std::make_unique<ClientPhishingRequest>();
  verdict->set_is_phishing(true);
  verdict->set_dom_model_version(100);

  cache_->InsertVerdict(url, std::move(verdict));

  EXPECT_NE(cache_->GetTotalVerdictEntriesSize(),
            total_entries_size_with_empty_verdict);
}

TEST_F(ClientSideDetectionFeatureCacheBaseTest,
       MaintainDebuggingMetadataMapCap) {
  std::string string_url = "https://www.testtest1234.com/";
  GURL url(string_url);

  LoginReputationClientRequest::DebuggingMetadata* metadata =
      cache_->GetOrCreateDebuggingMetadataForURL(url);
  ASSERT_NE(metadata, nullptr);
  EXPECT_EQ(cache_->GetDebuggingMetadataForURL(url), metadata);

  for (size_t count = 0; count < cache_->GetMaxMapCapacity() - 1; count++) {
    cache_->GetOrCreateDebuggingMetadataForURL(
        GURL(string_url + base::NumberToString(count)));
  }

  // The first metadata should still exist.
  EXPECT_EQ(cache_->GetDebuggingMetadataForURL(url), metadata);

  // Trigger eviction by adding one more.
  cache_->GetOrCreateDebuggingMetadataForURL(GURL("https://www.testtest.com"));

  // The first metadata should have been evicted.
  EXPECT_EQ(cache_->GetDebuggingMetadataForURL(url), nullptr);
}

TEST_F(ClientSideDetectionFeatureCacheBaseTest, DebuggingMetadataEntriesSize) {
  EXPECT_EQ(cache_->GetTotalDebuggingMetadataMapEntriesSize(), 0L);

  std::string string_url = "https://www.testtest1234.com/";

  for (size_t count = 0; count < cache_->GetMaxMapCapacity(); count++) {
    cache_->GetOrCreateDebuggingMetadataForURL(
        GURL(string_url + base::NumberToString(count)));
  }

  long total_entries_size_empty =
      cache_->GetTotalDebuggingMetadataMapEntriesSize();

  EXPECT_EQ(total_entries_size_empty, 0L);

  // Insert another one to trigger eviction.
  cache_->GetOrCreateDebuggingMetadataForURL(GURL(string_url + "0000"));

  EXPECT_EQ(cache_->GetTotalDebuggingMetadataMapEntriesSize(),
            total_entries_size_empty);
}

TEST_F(ClientSideDetectionFeatureCacheBaseTest, RemoveDebuggingMetadata) {
  GURL url("https://www.test.com");
  cache_->GetOrCreateDebuggingMetadataForURL(url);
  EXPECT_NE(cache_->GetDebuggingMetadataForURL(url), nullptr);

  cache_->RemoveDebuggingMetadataForURL(url);
  EXPECT_EQ(cache_->GetDebuggingMetadataForURL(url), nullptr);
}

TEST_F(ClientSideDetectionFeatureCacheBaseTest, ClearCacheClearsMetadata) {
  GURL url("https://www.test.com");
  cache_->GetOrCreateDebuggingMetadataForURL(url);
  EXPECT_NE(cache_->GetDebuggingMetadataForURL(url), nullptr);

  cache_->ClearForTesting();
  EXPECT_EQ(cache_->GetDebuggingMetadataForURL(url), nullptr);
}

TEST_F(ClientSideDetectionFeatureCacheBaseTest, UpdateVerdictEvictionOrder) {
  std::string string_url = "https://www.test.com/";
  GURL url_a(string_url + "a");

  // Insert A
  cache_->InsertVerdict(url_a, std::make_unique<ClientPhishingRequest>());

  // Fill the cache to capacity - 1
  for (size_t i = 0; i < cache_->GetMaxMapCapacity() - 1; ++i) {
    cache_->InsertVerdict(GURL(string_url + "b" + base::NumberToString(i)),
                          std::make_unique<ClientPhishingRequest>());
  }

  // Update A
  auto updated_verdict = std::make_unique<ClientPhishingRequest>();
  updated_verdict->set_dom_model_version(42);
  cache_->InsertVerdict(url_a, std::move(updated_verdict));

  // Insert one more item to trigger eviction
  GURL url_c(string_url + "c");
  cache_->InsertVerdict(url_c, std::make_unique<ClientPhishingRequest>());

  // Under FIFO (first insertion), A should be evicted because it was initially
  // inserted first.
  EXPECT_EQ(cache_->GetVerdictForURL(url_a), nullptr);
}

}  // namespace safe_browsing
