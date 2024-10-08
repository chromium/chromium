// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/remote_suggestions_service.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/search_engines/search_engines_test_environment.h"
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

using testing::_;
using testing::Invoke;
using testing::NiceMock;

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

class MockDelegate : public NiceMock<RemoteSuggestionsService::Delegate> {
 public:
  explicit MockDelegate(RemoteSuggestionsService* service) {
    service->SetDelegate(weak_ptr_factory_.GetWeakPtr());
  }
  ~MockDelegate() override = default;
  MockDelegate(const MockDelegate&) = delete;
  MockDelegate& operator=(const MockDelegate&) = delete;

  // RemoteSuggestionsService::Delegate:
  MOCK_METHOD(
      void,
      OnSuggestRequestCompleted,
      (const network::SimpleURLLoader* source,
       const int response_code,
       std::unique_ptr<std::string> response_body,
       RemoteSuggestionsService::CompletionCallback completion_callback),
      (override));
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
                         std::unique_ptr<std::string> response_body) {
    response_body_ = *response_body;
  }

  TemplateURLService& template_url_service() {
    return *search_engines_test_environment_.template_url_service();
  }

  std::string response_body() { return response_body_; }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
  std::string response_body_;
};

TEST_F(RemoteSuggestionsServiceTest, AttachCookies_ZeroPrefixSuggest) {
  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  RemoteSuggestionsService service(/*document_suggestions_service_=*/nullptr,
                                   GetUrlLoaderFactory());

  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.current_page_url = "https://www.google.com/";
  auto loader = service.StartZeroPrefixSuggestionsRequest(
      RemoteRequestType::kZeroSuggest,
      template_url_service().GetDefaultSearchProvider(), search_terms_args,
      template_url_service().search_terms_data(), base::DoNothing());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(net::LOAD_DO_NOT_SAVE_COOKIES, resource_request.load_flags);
  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(resource_request.url)))
      << resource_request.site_for_cookies.ToDebugString();
  const std::string kRequestUrl = "https://www.google.com/complete/search";
  EXPECT_EQ(kRequestUrl,
            resource_request.url.spec().substr(0, kRequestUrl.size()));
}

TEST_F(RemoteSuggestionsServiceTest, AttachCookies_Suggest) {
  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  RemoteSuggestionsService service(/*document_suggestions_service_=*/nullptr,
                                   GetUrlLoaderFactory());

  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.current_page_url = "https://www.google.com/";
  auto loader = service.StartSuggestionsRequest(
      RemoteRequestType::kSearch,
      template_url_service().GetDefaultSearchProvider(), search_terms_args,
      template_url_service().search_terms_data(), base::DoNothing());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(resource_request.url)))
      << resource_request.site_for_cookies.ToDebugString();
  const std::string kRequestUrl = "https://www.google.com/complete/search";
  EXPECT_EQ(kRequestUrl,
            resource_request.url.spec().substr(0, kRequestUrl.size()));
}

TEST_F(RemoteSuggestionsServiceTest, AttachCookies_DeleteSuggest) {
  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  RemoteSuggestionsService service(/*document_suggestions_service_=*/nullptr,
                                   GetUrlLoaderFactory());
  auto loader = service.StartDeletionRequest(
      "https://google.com/complete/delete", base::DoNothing());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(resource_request.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromUrl(resource_request.url)))
      << resource_request.site_for_cookies.ToDebugString();
}

TEST_F(RemoteSuggestionsServiceTest, BypassCache) {
  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  RemoteSuggestionsService service(/*document_suggestions_service_=*/nullptr,
                                   GetUrlLoaderFactory());

  TemplateURLRef::SearchTermsArgs search_terms_args;
  search_terms_args.current_page_url = "https://www.google.com/";
  search_terms_args.bypass_cache = true;
  auto loader = service.StartZeroPrefixSuggestionsRequest(
      RemoteRequestType::kZeroSuggest,
      template_url_service().GetDefaultSearchProvider(), search_terms_args,
      template_url_service().search_terms_data(), base::DoNothing());

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

TEST_F(RemoteSuggestionsServiceTest, Observer) {
  base::HistogramTester histogram_tester;

  TemplateURLData template_url_data;
  template_url_data.suggestions_url = "https://www.example.com/suggest";
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  RemoteSuggestionsService service(/*document_suggestions_service_=*/nullptr,
                                   GetUrlLoaderFactory());
  TestObserver observer(&service);
  auto loader = service.StartZeroPrefixSuggestionsRequest(
      RemoteRequestType::kZeroSuggest,
      template_url_service().GetDefaultSearchProvider(),
      TemplateURLRef::SearchTermsArgs(),
      template_url_service().search_terms_data(),
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

  // Verify the service client got notified of request completion.
  ASSERT_EQ(response_body(), kResponseBody);
}

TEST_F(RemoteSuggestionsServiceTest, Delegate) {
  base::HistogramTester histogram_tester;

  TemplateURLData template_url_data;
  template_url_data.suggestions_url = "https://www.example.com/suggest";
  template_url_service().SetUserSelectedDefaultSearchProvider(
      template_url_service().Add(
          std::make_unique<TemplateURL>(template_url_data)));

  RemoteSuggestionsService service(/*document_suggestions_service_=*/nullptr,
                                   GetUrlLoaderFactory());

  // Set up a delegate that will be replaced.
  MockDelegate delegate1(&service);
  EXPECT_CALL(delegate1, OnSuggestRequestCompleted(_, _, _, _)).Times(0);

  // Set up a delegate that will be deallocated.
  {
    MockDelegate delegate2(&service);
    EXPECT_CALL(delegate2, OnSuggestRequestCompleted(_, _, _, _)).Times(0);
  }

  // Set up a delegate that will call the completion callback asynchronously.
  MockDelegate delegate3(&service);
  EXPECT_CALL(delegate3, OnSuggestRequestCompleted(_, _, _, _))
      .WillOnce(Invoke(
          [](const network::SimpleURLLoader* source, const int response_code,
             std::unique_ptr<std::string> response_body,
             RemoteSuggestionsService::CompletionCallback completion_callback) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(completion_callback), source,
                               response_code, std::move(response_body)));
          }));

  auto loader = service.StartZeroPrefixSuggestionsRequest(
      RemoteRequestType::kZeroSuggest,
      template_url_service().GetDefaultSearchProvider(),
      TemplateURLRef::SearchTermsArgs(),
      template_url_service().search_terms_data(),
      base::BindOnce(&RemoteSuggestionsServiceTest::OnRequestComplete,
                     base::Unretained(this)));

  base::RunLoop().RunUntilIdle();

  // Verify the pending request and resolve it.
  const std::string kRequestUrl = "https://www.example.com/suggest";
  ASSERT_TRUE(test_url_loader_factory_.IsPending(kRequestUrl));
  const std::string kResponseBody = "example response";
  test_url_loader_factory_.AddResponse(kRequestUrl, kResponseBody);

  base::RunLoop().RunUntilIdle();

  // Verify the service client got notified of request completion.
  ASSERT_EQ(response_body(), kResponseBody);
}

TEST_F(RemoteSuggestionsServiceTest, CrOSOverridenOrAppendedQueryParams) {
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

TEST_F(RemoteSuggestionsServiceTest,
       LensOverlaySuggestInputsAppendedQueryParamsForContextualSearchbox) {
  // Set up a Google search provider.
  TemplateURLData google_template_url_data;
  google_template_url_data.SetURL(
      "https://www.google.com/search?q={searchTerms}");
  google_template_url_data.suggestions_url =
      "https://www.google.com/suggest?q={searchTerms}";
  google_template_url_data.id = SEARCH_ENGINE_GOOGLE;
  TemplateURL google_template_url(google_template_url_data);

  TemplateURLRef::SearchTermsArgs search_terms_args(u"query");
  search_terms_args.page_classification =
      metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX;
  search_terms_args.lens_overlay_suggest_inputs =
      std::make_optional<lens::proto::LensOverlaySuggestInputs>();
  search_terms_args.lens_overlay_suggest_inputs->set_encoded_image_signals(
      "iil");

  GURL endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &google_template_url, search_terms_args, SearchTermsData());

  // No additional query params is appended for empty Lens suggest inputs.
  // iil is not expected to be sent for contextual searchbox requests.
  ASSERT_EQ(endpoint_url.spec(),
            "https://www.google.com/suggest?q=query&client=chrome-contextual");

  search_terms_args.lens_overlay_suggest_inputs->set_encoded_request_id(
      "vsrid");
  search_terms_args.lens_overlay_suggest_inputs->set_search_session_id(
      "gsessionid");
  endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &google_template_url, search_terms_args, SearchTermsData());

  // No additional query params are appended for empty Lens suggest inputs
  // because send_gsession_vsrid_for_contextual_suggest is false.
  ASSERT_EQ(endpoint_url.spec(),
            "https://www.google.com/suggest?q=query&client=chrome-contextual");

  search_terms_args.lens_overlay_suggest_inputs
      ->set_send_gsession_vsrid_for_contextual_suggest(true);
  endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &google_template_url, search_terms_args, SearchTermsData());

  // Appended gsessionid and vsrids.
  ASSERT_EQ(endpoint_url.spec(),
            "https://www.google.com/"
            "suggest?q=query&client=chrome-contextual&vsrid=vsrid&gsessionid="
            "gsessionid");
}

TEST_F(RemoteSuggestionsServiceTest,
       LensOverlaySuggestInputsAppendedQueryParamsForLensSearchbox) {
  // Set up a Google search provider.
  TemplateURLData google_template_url_data;
  google_template_url_data.SetURL(
      "https://www.google.com/search?q={searchTerms}");
  google_template_url_data.suggestions_url =
      "https://www.google.com/suggest?q={searchTerms}";
  google_template_url_data.id = SEARCH_ENGINE_GOOGLE;
  TemplateURL google_template_url(google_template_url_data);

  TemplateURLRef::SearchTermsArgs search_terms_args(u"query");
  search_terms_args.page_classification =
      metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX;
  search_terms_args.lens_overlay_suggest_inputs =
      std::make_optional<lens::proto::LensOverlaySuggestInputs>();
  search_terms_args.lens_overlay_suggest_inputs->set_encoded_image_signals(
      "iil");

  GURL endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &google_template_url, search_terms_args, SearchTermsData());

  // Just iil query param appended.
  ASSERT_EQ(endpoint_url.spec(),
            "https://www.google.com/"
            "suggest?q=query&client=chrome-multimodal&iil=iil");

  search_terms_args.lens_overlay_suggest_inputs->set_encoded_request_id(
      "vsrid");
  search_terms_args.lens_overlay_suggest_inputs->set_search_session_id(
      "gsessionid");
  search_terms_args.lens_overlay_suggest_inputs
      ->set_encoded_visual_search_interaction_log_data("vsint");

  endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &google_template_url, search_terms_args, SearchTermsData());

  // No additional query params are appended for empty Lens suggest inputs
  // because send_gsession_vsrid_for_lens_suggest and
  // send_vsint_for_lens_suggest are false.
  ASSERT_EQ(endpoint_url.spec(),
            "https://www.google.com/"
            "suggest?q=query&client=chrome-multimodal&iil=iil");

  search_terms_args.lens_overlay_suggest_inputs
      ->set_send_gsession_vsrid_for_lens_suggest(true);
  endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &google_template_url, search_terms_args, SearchTermsData());

  // Appended gsessionid and vsrids.
  ASSERT_EQ(endpoint_url.spec(),
            "https://www.google.com/"
            "suggest?q=query&client=chrome-multimodal&iil=iil&vsrid=vsrid&"
            "gsessionid=gsessionid");

  search_terms_args.lens_overlay_suggest_inputs
      ->set_send_vsint_for_lens_suggest(true);
  endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &google_template_url, search_terms_args, SearchTermsData());

  // Appended vsint.
  ASSERT_EQ(endpoint_url.spec(),
            "https://www.google.com/"
            "suggest?q=query&client=chrome-multimodal&iil=iil&vsint=vsint&"
            "vsrid=vsrid&gsessionid=gsessionid");
}

TEST_F(RemoteSuggestionsServiceTest,
       LensOverlaySuggestInputsAppendedNothingForOtherPageClassifications) {
  // Set up a Google search provider.
  TemplateURLData google_template_url_data;
  google_template_url_data.SetURL(
      "https://www.google.com/search?q={searchTerms}");
  google_template_url_data.suggestions_url =
      "https://www.google.com/suggest?q={searchTerms}";
  google_template_url_data.id = SEARCH_ENGINE_GOOGLE;
  TemplateURL google_template_url(google_template_url_data);

  TemplateURLRef::SearchTermsArgs search_terms_args(u"query");
  search_terms_args.page_classification =
      metrics::OmniboxEventProto::NTP_REALBOX;
  search_terms_args.lens_overlay_suggest_inputs =
      std::make_optional<lens::proto::LensOverlaySuggestInputs>();
  search_terms_args.lens_overlay_suggest_inputs->set_encoded_image_signals(
      "iil");
  search_terms_args.lens_overlay_suggest_inputs->set_encoded_request_id(
      "vsrid");
  search_terms_args.lens_overlay_suggest_inputs->set_search_session_id(
      "gsessionid");
  search_terms_args.lens_overlay_suggest_inputs
      ->set_encoded_visual_search_interaction_log_data("vsint");
  search_terms_args.lens_overlay_suggest_inputs
      ->set_send_gsession_vsrid_for_contextual_suggest(true);
  search_terms_args.lens_overlay_suggest_inputs
      ->set_send_vsint_for_lens_suggest(true);
  search_terms_args.lens_overlay_suggest_inputs
      ->set_send_gsession_vsrid_for_lens_suggest(true);

  GURL endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &google_template_url, search_terms_args, SearchTermsData());

  // Nothing appended.
  ASSERT_EQ(endpoint_url.spec(), "https://www.google.com/suggest?q=query");
}

TEST_F(RemoteSuggestionsServiceTest,
       LensOverlaySuggestInputsAppendedNothingForNonGoogleSearch) {
  // Set up a non-Google search provider.
  TemplateURLData template_url_data;
  template_url_data.SetURL("https://www.example.com/search?q={searchTerms}");
  template_url_data.suggestions_url =
      "https://www.example.com/suggest?q={searchTerms}";
  TemplateURL template_url(template_url_data);

  TemplateURLRef::SearchTermsArgs search_terms_args(u"query");
  lens::proto::LensOverlaySuggestInputs lens_overlay_suggest_inputs;
  lens_overlay_suggest_inputs.set_encoded_image_signals("xyz");
  lens_overlay_suggest_inputs.set_encoded_request_id("vsrid");
  lens_overlay_suggest_inputs.set_search_session_id("gsessionid");
  lens_overlay_suggest_inputs.set_encoded_visual_search_interaction_log_data(
      "vsint");
  lens_overlay_suggest_inputs.set_send_gsession_vsrid_for_contextual_suggest(
      true);
  lens_overlay_suggest_inputs.set_send_vsint_for_lens_suggest(true);
  lens_overlay_suggest_inputs.set_send_gsession_vsrid_for_lens_suggest(true);
  search_terms_args.lens_overlay_suggest_inputs = lens_overlay_suggest_inputs;

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

  // No additional query params is appended for the non-multimodal searchbox
  // entry point for non-Google template URL.
  search_terms_args.page_classification =
      metrics::OmniboxEventProto::SEARCH_SIDE_PANEL_SEARCHBOX;
  endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &template_url, search_terms_args, SearchTermsData());
  ASSERT_EQ(endpoint_url.spec(), "https://www.example.com/suggest?q=query");

  // No additional query params is appended for the contextual searchbox entry
  // point for non-Google template URL.
  search_terms_args.page_classification =
      metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX;
  endpoint_url = RemoteSuggestionsService::EndpointUrl(
      &template_url, search_terms_args, SearchTermsData());
  ASSERT_EQ(endpoint_url.spec(), "https://www.example.com/suggest?q=query");
}
