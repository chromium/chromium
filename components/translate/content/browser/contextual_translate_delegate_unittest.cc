// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/contextual_translate_delegate.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/translate/content/browser/partial_translate_manager.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ContextualTranslateDelegateTest : public testing::Test {
 public:
  ContextualTranslateDelegateTest() = default;
  ~ContextualTranslateDelegateTest() override = default;

  void SetUp() override {
    test_url_loader_factory_ =
        std::make_unique<network::TestURLLoaderFactory>();
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            test_url_loader_factory_.get());
    delegate_ = std::make_unique<ContextualTranslateDelegate>(
        shared_url_loader_factory_);
  }

 protected:
  void StartPartialTranslate(
      const PartialTranslateRequest& request,
      base::test::TestFuture<PartialTranslateResponse>* future) {
    delegate_->StartPartialTranslate(
        request, nullptr,
        base::BindOnce(
            [](base::test::TestFuture<PartialTranslateResponse>* f,
               const PartialTranslateResponse& r) { f->SetValue(r); },
            future));
  }

  void StartPartialTranslateWithPrefs(
      const PartialTranslateRequest& request,
      PrefService* prefs,
      base::test::TestFuture<PartialTranslateResponse>* future) {
    delegate_->StartPartialTranslate(
        request, prefs,
        base::BindOnce(
            [](base::test::TestFuture<PartialTranslateResponse>* f,
               const PartialTranslateResponse& r) { f->SetValue(r); },
            future));
  }

  void WaitForRequest() {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return FindPendingRequestPrefix("https://translate-pa") != nullptr;
    }));
  }

  // Helper to find a pending request that starts with the given prefix.
  // Returns the pending request if found, otherwise nullptr.
  const network::ResourceRequest* FindPendingRequestPrefix(
      const std::string& prefix) {
    for (const auto& request : *test_url_loader_factory_->pending_requests()) {
      if (base::StartsWith(request.request.url.spec(), prefix,
                           base::CompareCase::SENSITIVE)) {
        return &request.request;
      }
    }
    return nullptr;
  }

  const network::ResourceRequest* FindPendingRequest(
      const std::string& url_exact) {
    for (const auto& request : *test_url_loader_factory_->pending_requests()) {
      if (base::StartsWith(request.request.url.spec(), url_exact,
                           base::CompareCase::SENSITIVE)) {
        return &request.request;
      }
    }
    return nullptr;
  }

  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<ContextualTranslateDelegate> delegate_;
};

TEST_F(ContextualTranslateDelegateTest, SuccessfulTranslation) {
  PartialTranslateRequest request;
  request.selection_text = u"Hello";
  request.source_language = "en";
  request.target_language = "es";

  base::test::TestFuture<PartialTranslateResponse> future;
  StartPartialTranslate(request, &future);

  WaitForRequest();

  const auto* pending_request = FindPendingRequest(
      "https://translate-pa.googleapis.com/v1/translateHtml");
  ASSERT_TRUE(pending_request);
  EXPECT_EQ(pending_request->method, "POST");

  GURL url = pending_request->url;

  ASSERT_TRUE(pending_request->request_body);
  ASSERT_EQ(1u, pending_request->request_body->elements()->size());
  const auto& element = pending_request->request_body->elements()->at(0);
  ASSERT_EQ(network::DataElement::Tag::kBytes, element.type());
  std::string post_data(
      element.As<network::DataElementBytes>().AsStringPiece());

  std::optional<base::Value> body_value =
      base::JSONReader::Read(post_data, base::JSON_PARSE_RFC);
  ASSERT_TRUE(body_value);
  ASSERT_TRUE(body_value->is_list());
  const auto& body_list = body_value->GetList();
  ASSERT_EQ(2u, body_list.size());

  const auto& query_set = body_list[0].GetList();
  ASSERT_EQ(3u, query_set.size());
  EXPECT_EQ("Hello", query_set[0].GetList()[0].GetString());
  EXPECT_EQ("en", query_set[1].GetString());
  EXPECT_EQ("es", query_set[2].GetString());

  EXPECT_EQ("chrome", body_list[1].GetString());
  std::optional<std::string> api_key =
      pending_request->headers.GetHeader("X-Goog-Api-Key");
  EXPECT_TRUE(api_key.has_value());
  EXPECT_FALSE(api_key->empty());

  const std::string response_body = R"([
    ["Hola"],
    ["en"]
  ])";

  ASSERT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
      url.spec(), response_body));

  PartialTranslateResponse response = future.Take();

  EXPECT_EQ(response.status, PartialTranslateStatus::kSuccess);
  EXPECT_EQ(response.translated_text, u"Hola");
  EXPECT_EQ(response.source_language, "en");
  EXPECT_EQ(response.target_language, "es");
}

TEST_F(ContextualTranslateDelegateTest, EmptyResponse) {
  PartialTranslateRequest request;
  request.selection_text = u"Hello";
  request.target_language = "es";

  base::test::TestFuture<PartialTranslateResponse> future;
  StartPartialTranslate(request, &future);

  WaitForRequest();

  const auto* pending_request = FindPendingRequest(
      "https://translate-pa.googleapis.com/v1/translateHtml");
  ASSERT_TRUE(pending_request);

  ASSERT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->url.spec(), ""));

  PartialTranslateResponse response = future.Take();

  EXPECT_EQ(response.status, PartialTranslateStatus::kError);
}

TEST_F(ContextualTranslateDelegateTest, MalformedJson) {
  PartialTranslateRequest request;
  request.selection_text = u"Hello";
  request.target_language = "es";

  base::test::TestFuture<PartialTranslateResponse> future;
  StartPartialTranslate(request, &future);

  WaitForRequest();

  const auto* pending_request = FindPendingRequest(
      "https://translate-pa.googleapis.com/v1/translateHtml");
  ASSERT_TRUE(pending_request);

  ASSERT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->url.spec(), "{ invalid json }"));

  PartialTranslateResponse response = future.Take();

  EXPECT_EQ(response.status, PartialTranslateStatus::kError);
}

TEST_F(ContextualTranslateDelegateTest, UnexpectedJsonStructure) {
  PartialTranslateRequest request;
  request.selection_text = u"Hello";
  request.target_language = "es";

  base::test::TestFuture<PartialTranslateResponse> future;
  StartPartialTranslate(request, &future);

  WaitForRequest();

  const auto* pending_request = FindPendingRequest(
      "https://translate-pa.googleapis.com/v1/translateHtml");
  ASSERT_TRUE(pending_request);

  // Valid JSON but missing "translation"
  const std::string response_body = R"([
  ])";

  ASSERT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->url.spec(), response_body));

  PartialTranslateResponse response = future.Take();

  EXPECT_EQ(response.status, PartialTranslateStatus::kError);
}

TEST_F(ContextualTranslateDelegateTest, MissingTranslatedText) {
  PartialTranslateRequest request;
  request.selection_text = u"Hello";
  request.target_language = "es";

  base::test::TestFuture<PartialTranslateResponse> future;
  StartPartialTranslate(request, &future);

  WaitForRequest();

  const auto* pending_request = FindPendingRequest(
      "https://translate-pa.googleapis.com/v1/translateHtml");
  ASSERT_TRUE(pending_request);

  const std::string response_body = R"([
    [],
    ["en"]
  ])";

  ASSERT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->url.spec(), response_body));

  PartialTranslateResponse response = future.Take();

  EXPECT_EQ(response.status, PartialTranslateStatus::kError);
}

TEST_F(ContextualTranslateDelegateTest, NetworkError) {
  PartialTranslateRequest request;
  request.selection_text = u"Hello";
  request.target_language = "es";

  base::test::TestFuture<PartialTranslateResponse> future;
  StartPartialTranslate(request, &future);

  WaitForRequest();

  const auto* pending_request = FindPendingRequest(
      "https://translate-pa.googleapis.com/v1/translateHtml");
  ASSERT_TRUE(pending_request);

  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->url, network::URLLoaderCompletionStatus(net::ERR_FAILED),
      network::mojom::URLResponseHead::New(), std::string());

  PartialTranslateResponse response = future.Take();

  EXPECT_EQ(response.status, PartialTranslateStatus::kError);
}

TEST_F(ContextualTranslateDelegateTest, HttpError) {
  PartialTranslateRequest request;
  request.selection_text = u"Hello";
  request.target_language = "es";

  base::test::TestFuture<PartialTranslateResponse> future;
  StartPartialTranslate(request, &future);

  WaitForRequest();

  const auto* pending_request = FindPendingRequest(
      "https://translate-pa.googleapis.com/v1/translateHtml");
  ASSERT_TRUE(pending_request);

  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->url.spec(), "", net::HTTP_INTERNAL_SERVER_ERROR);

  PartialTranslateResponse response = future.Take();

  EXPECT_EQ(response.status, PartialTranslateStatus::kError);
}

TEST_F(ContextualTranslateDelegateTest, JsonListResponse) {
  PartialTranslateRequest request;
  request.selection_text = u"Hello";
  request.target_language = "es";

  base::test::TestFuture<PartialTranslateResponse> future;
  StartPartialTranslate(request, &future);

  WaitForRequest();

  const auto* pending_request = FindPendingRequest(
      "https://translate-pa.googleapis.com/v1/translateHtml");
  ASSERT_TRUE(pending_request);

  // Response is a list, not a dict.
  const std::string response_body = R"(["translation", "Hola"])";

  ASSERT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->url.spec(), response_body));

  PartialTranslateResponse response = future.Take();

  EXPECT_EQ(response.status, PartialTranslateStatus::kError);
}

TEST_F(ContextualTranslateDelegateTest, TranslationWrongType) {
  PartialTranslateRequest request;
  request.selection_text = u"Hello";
  request.target_language = "es";

  base::test::TestFuture<PartialTranslateResponse> future;
  StartPartialTranslate(request, &future);

  WaitForRequest();

  const auto* pending_request = FindPendingRequest(
      "https://translate-pa.googleapis.com/v1/translateHtml");
  ASSERT_TRUE(pending_request);

  // "translation" is an integer, not a string.
  const std::string response_body = R"([
    [12345],
    ["en"]
  ])";

  ASSERT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->url.spec(), response_body));

  PartialTranslateResponse response = future.Take();

  EXPECT_EQ(response.status, PartialTranslateStatus::kError);
}

TEST_F(ContextualTranslateDelegateTest, SuccessNoSourceLanguage) {
  PartialTranslateRequest request;
  request.selection_text = u"Hello";
  request.target_language = "es";

  base::test::TestFuture<PartialTranslateResponse> future;
  StartPartialTranslate(request, &future);

  WaitForRequest();

  const auto* pending_request = FindPendingRequest(
      "https://translate-pa.googleapis.com/v1/translateHtml");
  ASSERT_TRUE(pending_request);

  const std::string response_body = R"([
    ["Hola"]
  ])";

  ASSERT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->url.spec(), response_body));

  PartialTranslateResponse response = future.Take();

  EXPECT_EQ(response.status, PartialTranslateStatus::kSuccess);
  EXPECT_EQ(response.translated_text, u"Hola");
  EXPECT_TRUE(response.source_language.empty());
  EXPECT_EQ(response.target_language, "es");
}

TEST_F(ContextualTranslateDelegateTest, UrlTooLong) {
  PartialTranslateRequest request;
  // 10KB limit. Generate a string that when encoded exceeds this.
  // 10240 chars.
  std::u16string long_text(11000, 'a');
  request.selection_text = long_text;
  request.target_language = "es";

  base::test::TestFuture<PartialTranslateResponse> future;
  StartPartialTranslate(request, &future);

  WaitForRequest();
  const auto* pending_request = FindPendingRequest(
      "https://translate-pa.googleapis.com/v1/translateHtml");
  // We will always make a request, even if the URL is too long to enforce 414
  // response So this will be correctly logged by UMA metrics.
  ASSERT_TRUE(pending_request);

  test_url_loader_factory_->SimulateResponseForPendingRequest(
      pending_request->url.spec(), "", net::HTTP_REQUEST_URI_TOO_LONG);

  PartialTranslateResponse response = future.Take();

  EXPECT_EQ(response.status, PartialTranslateStatus::kError);
}

TEST_F(ContextualTranslateDelegateTest, RegionalEndpoints) {
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterIntegerPref(
      translate::prefs::kTranslateDataRegionSetting, 0);

  PartialTranslateRequest request;
  request.selection_text = u"Hello";
  request.source_language = "en";
  request.target_language = "es";

  // Test US region.
  prefs.SetInteger(translate::prefs::kTranslateDataRegionSetting, 1);
  {
    base::test::TestFuture<PartialTranslateResponse> future;
    StartPartialTranslateWithPrefs(request, &prefs, &future);
    WaitForRequest();
    const auto* pending_request =
        FindPendingRequestPrefix("https://translate-pa.us.rep.googleapis.com");
    ASSERT_TRUE(pending_request);

    ASSERT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
        pending_request->url.spec(), R"([["Hola"]])"));
    EXPECT_EQ(future.Take().status, PartialTranslateStatus::kSuccess);
  }

  // Test EU region.
  prefs.SetInteger(translate::prefs::kTranslateDataRegionSetting, 2);
  {
    base::test::TestFuture<PartialTranslateResponse> future;
    StartPartialTranslateWithPrefs(request, &prefs, &future);
    WaitForRequest();
    const auto* pending_request =
        FindPendingRequestPrefix("https://translate-pa.eu.rep.googleapis.com");
    ASSERT_TRUE(pending_request);

    ASSERT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
        pending_request->url.spec(), R"([["Hola"]])"));
    EXPECT_EQ(future.Take().status, PartialTranslateStatus::kSuccess);
  }

  // Test default NO_PREFERENCE region.
  prefs.SetInteger(translate::prefs::kTranslateDataRegionSetting, 0);
  {
    base::test::TestFuture<PartialTranslateResponse> future;
    StartPartialTranslateWithPrefs(request, &prefs, &future);
    WaitForRequest();
    const auto* pending_request =
        FindPendingRequestPrefix("https://translate-pa.googleapis.com");
    ASSERT_TRUE(pending_request);

    ASSERT_TRUE(test_url_loader_factory_->SimulateResponseForPendingRequest(
        pending_request->url.spec(), R"([["Hola"]])"));
    EXPECT_EQ(future.Take().status, PartialTranslateStatus::kSuccess);
  }
}

TEST_F(ContextualTranslateDelegateTest, DestroyedDuringCancellation) {
  PartialTranslateRequest request1;
  request1.selection_text = u"Hello 1";
  request1.target_language = "es";

  PartialTranslateRequest request2;
  request2.selection_text = u"Hello 2";
  request2.target_language = "fr";

  bool callback1_ran = false;

  // Start the first request.
  delegate_->StartPartialTranslate(
      request1, nullptr,
      base::BindOnce(
          [](std::unique_ptr<ContextualTranslateDelegate>* delegate_ptr,
             bool* ran, const PartialTranslateResponse& response) {
            *ran = true;
            EXPECT_EQ(response.status, PartialTranslateStatus::kError);
            // Destroy the delegate during the callback execution!
            delegate_ptr->reset();
          },
          &delegate_, &callback1_ran));

  WaitForRequest();

  // Start the second request. This will cancel the first request, immediately
  // triggering the callback, which in turn deletes the delegate.
  // Because StartPartialTranslate checks the return value of
  // CancelPendingRequest(), it should safely abort without crashing from a
  // use-after-free.
  delegate_->StartPartialTranslate(request2, nullptr, base::DoNothing());

  EXPECT_TRUE(callback1_ran);
  EXPECT_FALSE(delegate_);
}

TEST_F(ContextualTranslateDelegateTest, PayloadWithLangHint) {
  translate::TranslateDownloadManager::GetInstance()->set_application_locale(
      "en-US");

  PartialTranslateRequest request;
  request.selection_text = u"Hello";
  request.source_language = "en";
  request.target_language = "es";
  request.apply_lang_hint = true;

  base::test::TestFuture<PartialTranslateResponse> future;
  StartPartialTranslate(request, &future);

  WaitForRequest();

  const auto* pending_request = FindPendingRequest(
      "https://translate-pa.googleapis.com/v1/translateHtml");
  ASSERT_TRUE(pending_request);

  ASSERT_TRUE(pending_request->request_body);
  ASSERT_EQ(1u, pending_request->request_body->elements()->size());
  const auto& element = pending_request->request_body->elements()->at(0);
  std::string post_data(
      element.As<network::DataElementBytes>().AsStringPiece());

  std::optional<base::Value> body_value =
      base::JSONReader::Read(post_data, base::JSON_PARSE_RFC);
  ASSERT_TRUE(body_value);
  const auto& body_list = body_value->GetList();
  const auto& query_set = body_list[0].GetList();

  // With apply_lang_hint = true, source_language should be omitted (replaced
  // with a null value) and the application_locale should be appended at the
  // end.
  ASSERT_EQ(4u, query_set.size());
  EXPECT_EQ("Hello", query_set[0].GetList()[0].GetString());
  EXPECT_TRUE(query_set[1].is_none());
  EXPECT_EQ("es", query_set[2].GetString());
  EXPECT_EQ("en-US", query_set[3].GetString());
}

}  // namespace
