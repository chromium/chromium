// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/site_affiliation/affiliation_fetcher_factory_impl.h"

#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher_interface.h"
#include "components/password_manager/core/browser/android_affiliation/mock_affiliation_fetcher_delegate.h"
#include "components/password_manager/core/common/password_manager_features.h"
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

  void EnableHashAffiliationService() {
    feature_list_.InitAndEnableFeature(
        password_manager::features::kUseOfHashAffiliationFetcher);
  }

  void DisableHashAffiliationService() {
    feature_list_.InitAndDisableFeature(
        password_manager::features::kUseOfHashAffiliationFetcher);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  AffiliationFetcherFactoryImpl fetcher_factory_;
  GURL url_;
};

TEST_F(AffiliationFetcherFactoryImplTest, NormalAffiliationFetcher) {
  DisableHashAffiliationService();
  CreateAndStartFetcher();
  EXPECT_TRUE(base::EndsWith(url().path_piece(), "lookup"));
}

TEST_F(AffiliationFetcherFactoryImplTest, HashAffiliationFetcher) {
  EnableHashAffiliationService();
  CreateAndStartFetcher();
  EXPECT_TRUE(base::EndsWith(url().path_piece(), "lookupByHashPrefix"));
}

}  // namespace password_manager
