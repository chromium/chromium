// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/remote_suggestions_service.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestObserver : public RemoteSuggestionsService::Observer {
 public:
  explicit TestObserver(RemoteSuggestionsService* service) : service_(service) {
    service_->AddObserver(this);
  }
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override { service_->RemoveObserver(this); }

  base::UnguessableToken request_id() { return request_id_; }
  GURL url() { return url_; }
  bool response_received() { return response_received_; }
  std::string response_body() { return response_body_; }

  // RemoteSuggestionsService::Observer:
  void OnSuggestRequestStarting(
      const base::UnguessableToken& request_id,
      const network::ResourceRequest* request) override {
    request_id_ = request_id;
    url_ = request->url;
  }
  void OnSuggestRequestCompleted(
      const base::UnguessableToken& request_id,
      const bool response_received,
      const std::unique_ptr<std::string>& response_body) override {
    // Verify the observer has been notified of this request.
    ASSERT_EQ(request_id_, request_id);
    response_received_ = true;
    response_body_ = *response_body;
  }

 private:
  raw_ptr<RemoteSuggestionsService> service_;
  base::UnguessableToken request_id_;
  GURL url_;
  bool response_received_{false};
  std::string response_body_;
};

}  // namespace

class RemoteSuggestionsServiceTest : public testing::Test {
 public:
  RemoteSuggestionsServiceTest() = default;

  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory() {
    return test_url_loader_factory_.GetSafeWeakWrapper();
  }

  void OnRequestComplete(const network::SimpleURLLoader* source,
                         const bool response_received,
                         std::unique_ptr<std::string> response_body) {}

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(RemoteSuggestionsServiceTest, EnsureAttachCookies) {
  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  RemoteSuggestionsService service(GetUrlLoaderFactory());
  TemplateURLService template_url_service(nullptr, 0);
  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.current_page_url = "https://www.google.com/";
  service.StartZeroPrefixSuggestionsRequest(
      template_url_service.GetDefaultSearchProvider(), search_terms_args,
      template_url_service.search_terms_data(),
      base::BindOnce(&RemoteSuggestionsServiceTest::OnRequestComplete,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::LOAD_DO_NOT_SAVE_COOKIES, resource_request.load_flags);
  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(resource_request.url)));
  const std::string kRequestUrl = "https://www.google.com/complete/search";
  EXPECT_EQ(kRequestUrl,
            resource_request.url.spec().substr(0, kRequestUrl.size()));
}

TEST_F(RemoteSuggestionsServiceTest, EnsureBypassCache) {
  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  RemoteSuggestionsService service(GetUrlLoaderFactory());
  TemplateURLService template_url_service(nullptr, 0);
  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.current_page_url = "https://www.google.com/";
  search_terms_args.bypass_cache = true;
  service.StartZeroPrefixSuggestionsRequest(
      template_url_service.GetDefaultSearchProvider(), search_terms_args,
      template_url_service.search_terms_data(),
      base::BindOnce(&RemoteSuggestionsServiceTest::OnRequestComplete,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_BYPASS_CACHE,
            resource_request.load_flags);
  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(resource_request.url)));
  const std::string kRequestUrl = "https://www.google.com/complete/search";
  EXPECT_EQ(kRequestUrl,
            resource_request.url.spec().substr(0, kRequestUrl.size()));
}

TEST_F(RemoteSuggestionsServiceTest, EnsureObservers) {
  TemplateURLService template_url_service(nullptr, 0);
  TemplateURLData template_url_data;
  template_url_data.suggestions_url = "https://www.example.com/suggest";
  template_url_service.SetUserSelectedDefaultSearchProvider(
      template_url_service.Add(
          std::make_unique<TemplateURL>(template_url_data)));

  RemoteSuggestionsService service(GetUrlLoaderFactory());

  TestObserver observer(&service);

  auto loader = service.StartZeroPrefixSuggestionsRequest(
      template_url_service.GetDefaultSearchProvider(),
      TemplateURLRef::SearchTermsArgs(),
      template_url_service.search_terms_data(),
      base::BindOnce(&RemoteSuggestionsServiceTest::OnRequestComplete,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // Verify the observer got notified of request start.
  const std::string kRequestUrl = "https://www.example.com/suggest";
  ASSERT_EQ(observer.url().spec(), kRequestUrl);
  ASSERT_FALSE(observer.response_received());

  // Verify the pending request and resolve it.
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kRequestUrl));
  const std::string kResponseBody = "example response";
  test_url_loader_factory_.AddResponse(kRequestUrl, kResponseBody);

  base::RunLoop().RunUntilIdle();

  // Verify the observer got notified of request completion.
  ASSERT_EQ(observer.url().spec(), kRequestUrl);
  ASSERT_TRUE(observer.response_received());
  ASSERT_EQ(observer.response_body(), kResponseBody);
}
