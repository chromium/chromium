// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/site_affiliation/hash_affiliation_fetcher.h"

#include "components/password_manager/core/browser/android_affiliation/mock_affiliation_fetcher_delegate.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {

class HashAffiliationFetcherTest : public testing::Test {
 public:
  HashAffiliationFetcherTest() = default;
  ~HashAffiliationFetcherTest() override = default;
};

TEST_F(HashAffiliationFetcherTest, BuildQueryURL) {
  network::TestURLLoaderFactory test_url_loader_factory;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory);
  MockAffiliationFetcherDelegate mock_delegate;

  HashAffiliationFetcher fetcher(test_shared_loader_factory, &mock_delegate);

  EXPECT_EQ(GURL("https://www.googleapis.com/affiliation/v1/"
                 "affiliation:lookupByHashPrefix?key=dummytoken"),
            fetcher.BuildQueryURL());
}

}  // namespace password_manager
