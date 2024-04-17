// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/remote_suggestions_service.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
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
#include "third_party/metrics_proto/omnibox_event.pb.h"

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
  void OnSuggestRequestCreated(
      const base::UnguessableToken& request_id,
      const network::ResourceRequest* request) override {
    request_id_ = request_id;
    url_ = request->url;
  }
  void OnSuggestRequestStarted(const base::UnguessableToken& request_id,
                               network::SimpleURLLoader* loader,
                               const std::string& request_body) override {
    ASSERT_EQ(request_id_, request_id);
  }
  void OnSuggestRequestCompleted(
      const base::UnguessableToken& request_id,
      const int response_code,
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
                         const int response_code,
                         std::unique_ptr<std::string> response_body) {}

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(RemoteSuggestionsServiceTest, EnsureAttachCookies_ZeroPrefixSuggest) {
  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  RemoteSuggestionsService service(/*document_suggestions_service_=*/nullptr,
                                   GetUrlLoaderFactory());
  TemplateURLService template_url_service(
      /*prefs=*/nullptr, /*search_engine_choice_service=*/nullptr);
  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.current_page_url = "https://www.google.com/";
  auto loader = service.StartZeroPrefixSuggestionsRequest(
      RemoteRequestType::kZeroSuggest,
      template_url_service.GetDefaultSearchProvider(), search_terms_args,
      template_url_service.search_terms_data(),
      base::BindOnce(&RemoteSuggestionsServiceTest::OnRequestComplete,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::LOAD_DO_NOT_SAVE_COOKIES, resource_request.load_flags);
  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(resource_request.url)))
      << resource_request.site_for_cookies.ToDebugString();
  const std::string kRequestUrl = "https://www.google.com/complete/search";
  EXPECT_EQ(kRequestUrl,
            resource_request.url.spec().substr(0, kRequestUrl.size()));
}

TEST_F(RemoteSuggestionsServiceTest, EnsureAttachCookies_Suggest) {
  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  RemoteSuggestionsService service(/*document_suggestions_service_=*/nullptr,
                                   GetUrlLoaderFactory());
  TemplateURLService template_url_service(
      /*prefs=*/nullptr, /*search_engine_choice_service=*/nullptr);
  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.current_page_url = "https://www.google.com/";
  auto loader = service.StartSuggestionsRequest(
      RemoteRequestType::kSearch,
      template_url_service.GetDefaultSearchProvider(), search_terms_args,
      template_url_service.search_terms_data(),
      base::BindOnce(&RemoteSuggestionsServiceTest::OnRequestComplete,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(resource_request.url)))
      << resource_request.site_for_cookies.ToDebugString();
  const std::string kRequestUrl = "https://www.google.com/complete/search";
  EXPECT_EQ(kRequestUrl,
            resource_request.url.spec().substr(0, kRequestUrl.size()));
}

TEST_F(RemoteSuggestionsServiceTest, EnsureAttachCookies_DeleteSuggest) {
  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  RemoteSuggestionsService service(/*document_suggestions_service_=*/nullptr,
                                   GetUrlLoaderFactory());
  auto loader = service.StartDeletionRequest(
      "https://google.com/complete/delete",
      base::BindOnce(&RemoteSuggestionsServiceTest::OnRequestComplete,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(resource_request.url)))
      << resource_request.site_for_cookies.ToDebugString();
}

TEST_F(RemoteSuggestionsServiceTest, EnsureBypassCache) {
  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  RemoteSuggestionsService service(/*document_suggestions_service_=*/nullptr,
                                   GetUrlLoaderFactory());
  TemplateURLService template_url_service(
      /*prefs=*/nullptr, /*search_engine_choice_service=*/nullptr);
  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.current_page_url = "https://www.google.com/";
  search_terms_args.bypass_cache = true;
  auto loader = service.StartZeroPrefixSuggestionsRequest(
      RemoteRequestType::kZeroSuggest,
      template_url_service.GetDefaultSearchProvider(), search_terms_args,
      template_url_service.search_terms_data(),
      base::BindOnce(&RemoteSuggestionsServiceTest::OnRequestComplete,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::LOAD_DO_NOT_SAVE_COOKIES | net::LOAD_BYPASS_CACHE,
            resource_request.load_flags);
  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(resource_request.url)))
      << resource_request.site_for_cookies.ToDebugString();
  const std::string kRequestUrl = "https://www.google.com/complete/search";
  EXPECT_EQ(kRequestUrl,
            resource_request.url.spec().substr(0, kRequestUrl.size()));
}

TEST_F(RemoteSuggestionsServiceTest, EnsureObservers) {
  base::HistogramTester histogram_tester;

  TemplateURLService template_url_service(
      /*prefs=*/nullptr, /*search_engine_choice_service=*/nullptr);
  TemplateURLData template_url_data;
  template_url_data.suggestions_url = "https://www.example.com/suggest";
  template_url_service.SetUserSelectedDefaultSearchProvider(
      template_url_service.Add(
          std::make_unique<TemplateURL>(template_url_data)));

  RemoteSuggestionsService service(/*document_suggestions_service_=*/nullptr,
                                   GetUrlLoaderFactory());
  TestObserver observer(&service);
  auto loader = service.StartZeroPrefixSuggestionsRequest(
      RemoteRequestType::kZeroSuggest,
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

  // Verify histogram was recorded.
  histogram_tester.ExpectTotalCount("Omnibox.SuggestRequestsSent", 1);
  histogram_tester.ExpectBucketCount("Omnibox.SuggestRequestsSent", 3, 1);

  // Verify the observer got notified of request completion.
  ASSERT_EQ(observer.url().spec(), kRequestUrl);
  ASSERT_TRUE(observer.response_received());
  ASSERT_EQ(observer.response_body(), kResponseBody);
}

TEST_F(RemoteSuggestionsServiceTest, EnsureCrOSOverridenOrAppendedQueryParams) {
  // Set up a non-Google search provider.
  TemplateURLData template_url_data;
  template_url_data.SetURL("https://www.example.com/search?q={searchTerms}");
  template_url_data.suggestions_url =
      "https://www.example.com/suggest?q={searchTerms}";
  TemplateURL template_url(template_url_data);

  TemplateURLRef::SearchTermsArgs search_terms_args(u"query");
  search_terms_args.page_classification =
      metrics::OmniboxEventProto::NTP_REALBOX;

  GURL endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &template_url, search_terms_args, SearchTermsData());

  // No additional query params is appended for the realbox entry point.
  ASSERT_EQ(endpoint_url.spec(), "https://www.example.com/suggest?q=query");

  // No additional query params is appended for the ChromeOS app_list launcher
  // entry point for non-Google template URL.
  search_terms_args.page_classification =
      metrics::OmniboxEventProto::CHROMEOS_APP_LIST;
  endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &template_url, search_terms_args, SearchTermsData());
  ASSERT_EQ(endpoint_url.spec(), "https://www.example.com/suggest?q=query");

  // Set up a Google search provider.
  TemplateURLData google_template_url_data;
  google_template_url_data.SetURL(
      "https://www.google.com/search?q={searchTerms}");
  google_template_url_data.suggestions_url =
      "https://www.google.com/suggest?q={searchTerms}";
  google_template_url_data.id = SEARCH_ENGINE_GOOGLE;
  TemplateURL google_template_url(google_template_url_data);

  // `sclient=` is appended for the ChromeOS app_list launcher entry point for
  // Google template URL.
  endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &google_template_url, search_terms_args, SearchTermsData());
  ASSERT_EQ(endpoint_url.spec(),
            "https://www.google.com/suggest?q=query&sclient=cros-launcher");
}

TEST_F(RemoteSuggestionsServiceTest, EnsureLensOverridenOrAppendedQueryParams) {
  // Set up a non-Google search provider.
  TemplateURLData template_url_data;
  template_url_data.SetURL("https://www.example.com/search?q={searchTerms}");
  template_url_data.suggestions_url =
      "https://www.example.com/suggest?q={searchTerms}";
  TemplateURL template_url(template_url_data);

  TemplateURLRef::SearchTermsArgs search_terms_args(u"query");
  lens::proto::LensOverlayInteractionResponse lens_overlay_interaction_response;
  lens_overlay_interaction_response.set_suggest_signals("xyz");
  search_terms_args.lens_overlay_interaction_response =
      lens_overlay_interaction_response;

  search_terms_args.page_classification =
      metrics::OmniboxEventProto::NTP_REALBOX;

  GURL endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &template_url, search_terms_args, SearchTermsData());

  // No additional query params is appended for the realbox entry point.
  ASSERT_EQ(endpoint_url.spec(), "https://www.example.com/suggest?q=query");

  // No additional query params is appended for the multimodal searchbox entry
  // point for non-Google template URL.
  search_terms_args.page_classification =
      metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX;
  endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &template_url, search_terms_args, SearchTermsData());
  ASSERT_EQ(endpoint_url.spec(), "https://www.example.com/suggest?q=query");

  // Set up a Google search provider.
  TemplateURLData google_template_url_data;
  google_template_url_data.SetURL(
      "https://www.google.com/search?q={searchTerms}");
  google_template_url_data.suggestions_url =
      "https://www.google.com/suggest?q={searchTerms}";
  google_template_url_data.id = SEARCH_ENGINE_GOOGLE;
  TemplateURL google_template_url(google_template_url_data);

  // `iil=` is appended for the for the multimodal searchbox entry point for
  // Google template URL.
  endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &google_template_url, search_terms_args, SearchTermsData());
  ASSERT_EQ(endpoint_url.spec(),
            "https://www.google.com/suggest?q=query&iil=xyz");
}
