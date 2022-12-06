// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/affiliation_fetcher_factory_impl.h"

#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/affiliation/affiliation_fetcher_interface.h"
#include "components/password_manager/core/browser/affiliation/mock_affiliation_fetcher_delegate.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

class AffiliationFetcherFactoryImplTest : public testing::Test {
 public:
  AffiliationFetcherFactoryImplTest() {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) { url_ = request.url; }));
  }

  void CreateAndStartFetcher() {
    MockAffiliationFetcherDelegate mock_delegate;
    std::unique_ptr<AffiliationFetcherInterface> fetcher =
        fetcher_factory_.CreateInstance(test_shared_loader_factory_,
                                        &mock_delegate);

    fetcher->StartRequest({}, {});
  }

  const GURL& url() const { return url_; }

 private:
  base::test::TaskEnvironment task_env_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  AffiliationFetcherFactoryImpl fetcher_factory_;
  GURL url_;
};

TEST_F(AffiliationFetcherFactoryImplTest, HashAffiliationFetcher) {
  CreateAndStartFetcher();
  EXPECT_TRUE(base::EndsWith(url().path_piece(), "lookupByHashPrefix"));
}

}  // namespace password_manager
