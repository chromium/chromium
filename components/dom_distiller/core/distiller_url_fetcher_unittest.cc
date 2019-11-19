// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distiller_url_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

const char kTestPageA[] = "http://www.a.com/";
const char kTestPageAResponse[] = {1, 2, 3, 4, 5, 6, 7};
const char kTestPageB[] = "http://www.b.com/";
const char kTestPageBResponse[] = {'a', 'b', 'c'};

class DistillerURLFetcherTest : public testing::Test {
 public:
  DistillerURLFetcherTest()
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void FetcherCallback(const std::string& response) { response_ = response; }

 protected:
  // testing::Test implementation:
  void SetUp() override {
    url_fetcher_.reset(new dom_distiller::DistillerURLFetcher(
        test_shared_url_loader_factory_));
    test_url_loader_factory_.AddResponse(
        kTestPageA,
        std::string(kTestPageAResponse, sizeof(kTestPageAResponse)));
    test_url_loader_factory_.AddResponse(
        GURL(kTestPageB),
        network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR),
        std::string(kTestPageBResponse, sizeof(kTestPageBResponse)),
        network::URLLoaderCompletionStatus(net::OK));
  }

  void Fetch(const std::string& url, const std::string& expected_response) {
    url_fetcher_->FetchURL(url,
                           base::Bind(&DistillerURLFetcherTest::FetcherCallback,
                                      base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    CHECK_EQ(expected_response, response_);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  std::unique_ptr<dom_distiller::DistillerURLFetcher> url_fetcher_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  std::string response_;
};

TEST_F(DistillerURLFetcherTest, PopulateProto) {
  Fetch(kTestPageA,
        std::string(kTestPageAResponse, sizeof(kTestPageAResponse)));
}

TEST_F(DistillerURLFetcherTest, PopulateProtoFailedFetch) {
  // Expect the callback to contain an empty string for this URL.
  Fetch(kTestPageB, std::string(std::string(), 0));
}
