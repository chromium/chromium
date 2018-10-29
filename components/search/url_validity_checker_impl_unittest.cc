// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/url_validity_checker_impl.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;

class UrlValidityCheckerImplTest : public testing::Test {
 protected:
  UrlValidityCheckerImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)),
        url_checker_(test_shared_loader_factory_,
                     scoped_task_environment_.GetMockTickClock()) {}

  ~UrlValidityCheckerImplTest() override {}

  void SetUp() override {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory());
  }

  UrlValidityCheckerImpl* url_checker() { return &url_checker_; }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &test_url_loader_factory_;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  UrlValidityCheckerImpl url_checker_;

  DISALLOW_COPY_AND_ASSIGN(UrlValidityCheckerImplTest);
};

TEST_F(UrlValidityCheckerImplTest, DoesUrlResolve_OnSuccess) {
  const GURL kUrl("https://www.foo.com");
  const int kTimeAdvance = 10;
  base::TimeDelta expected_duration =
      base::TimeDelta::FromSeconds(kTimeAdvance);

  network::ResourceResponseHead response;
  response.headers = new net::HttpResponseHeaders(
      "HTTP/1.1 200 OK\nContent-type: text/html\n\n");
  url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        scoped_task_environment_.FastForwardBy(expected_duration);
        url_loader_factory()->AddResponse(
            request.url, response, std::string(),
            network::URLLoaderCompletionStatus(net::OK));
      }));
  base::MockCallback<UrlValidityChecker::UrlValidityCheckerCallback>
      callback_ok;
  EXPECT_CALL(callback_ok, Run(true, expected_duration));

  url_checker()->DoesUrlResolve(kUrl, TRAFFIC_ANNOTATION_FOR_TESTS,
                                callback_ok.Get());
  scoped_task_environment_.RunUntilIdle();

  response.headers =
      new net::HttpResponseHeaders("HTTP/1.1 204 No Content\r\n\r\n");
  base::MockCallback<UrlValidityChecker::UrlValidityCheckerCallback>
      callback_no_content;
  EXPECT_CALL(callback_no_content, Run(true, expected_duration));

  url_checker()->DoesUrlResolve(kUrl, TRAFFIC_ANNOTATION_FOR_TESTS,
                                callback_no_content.Get());
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(UrlValidityCheckerImplTest, DoesUrlResolve_OnFailure) {
  const GURL kUrl("https://www.foo.com");
  const int kTimeAdvance = 20;
  base::TimeDelta expected_duration =
      base::TimeDelta::FromSeconds(kTimeAdvance);

  url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        scoped_task_environment_.FastForwardBy(expected_duration);
        url_loader_factory()->AddResponse(
            request.url, network::ResourceResponseHead(), std::string(),
            network::URLLoaderCompletionStatus(net::ERR_FAILED));
      }));
  base::MockCallback<UrlValidityChecker::UrlValidityCheckerCallback> callback;
  EXPECT_CALL(callback, Run(false, expected_duration));

  url_checker()->DoesUrlResolve(kUrl, TRAFFIC_ANNOTATION_FOR_TESTS,
                                callback.Get());
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(UrlValidityCheckerImplTest, DoesUrlResolve_OnRedirect) {
  const GURL kUrl("https://www.foo.com");
  const GURL kRedirectUrl("https://www.foo2.com");
  const int kTimeAdvance = 30;
  base::TimeDelta expected_duration =
      base::TimeDelta::FromSeconds(kTimeAdvance);

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = kRedirectUrl;
  network::TestURLLoaderFactory::Redirects redirects{
      {redirect_info, network::ResourceResponseHead()}};
  url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        scoped_task_environment_.FastForwardBy(expected_duration);
        url_loader_factory()->AddResponse(
            request.url, network::ResourceResponseHead(), std::string(),
            network::URLLoaderCompletionStatus(), redirects);
      }));
  base::MockCallback<UrlValidityChecker::UrlValidityCheckerCallback> callback;
  EXPECT_CALL(callback, Run(true, expected_duration));

  url_checker()->DoesUrlResolve(kUrl, TRAFFIC_ANNOTATION_FOR_TESTS,
                                callback.Get());
  scoped_task_environment_.RunUntilIdle();
}

TEST_F(UrlValidityCheckerImplTest, DoesUrlResolve_OnTimeout) {
  const GURL kUrl("https://www.foo.com");

  base::MockCallback<UrlValidityChecker::UrlValidityCheckerCallback> callback;
  EXPECT_CALL(callback, Run(false, _));

  url_checker()->DoesUrlResolve(kUrl, TRAFFIC_ANNOTATION_FOR_TESTS,
                                callback.Get());
  scoped_task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  scoped_task_environment_.RunUntilIdle();
}
