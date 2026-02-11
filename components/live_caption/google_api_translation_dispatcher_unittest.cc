// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/google_api_translation_dispatcher.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace captions {

using testing::_;
using testing::Eq;

class GoogleApiTranslationDispatcherTest : public testing::Test {
 protected:
  void SetUp() override {
    test_url_loader_factory_.Clone(
        remote_url_loader_factory_.BindNewPipeAndPassReceiver());

    translation_dispatcher_ = std::make_unique<GoogleApiTranslationDispatcher>(
        "test_api_key", nullptr);

    translation_dispatcher_->SetURLLoaderFactoryForTest(  // IN-TEST
        std::move(remote_url_loader_factory_));
  }

  network::TestURLLoaderFactory::PendingRequest* GetPendingRequest() {
    return test_url_loader_factory_.GetPendingRequest(0);
  }

  void GetTranslation(const std::string& result,
                      std::string source_language,
                      std::string target_language,
                      base::MockCallback<TranslateEventCallback>& callback) {
    translation_dispatcher_->GetTranslation(result, source_language,
                                            target_language, callback.Get());
  }

  void SimulateSuccessfulResponse(const std::string& response_body) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
    head->mime_type = "application/json";
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetPendingRequest()->request.url,
        network::URLLoaderCompletionStatus(net::OK), std::move(head),
        response_body);
  }

  void SimulateResponseForPendingRequest(
      const network::URLLoaderCompletionStatus& status,
      network::mojom::URLResponseHeadPtr head,
      const std::string& body) {
    test_url_loader_factory_.SimulateResponseForPendingRequest(
        GetPendingRequest()->request.url,
        network::URLLoaderCompletionStatus(net::ERR_FAILED),
        network::mojom::URLResponseHead::New(), std::string());
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  mojo::Remote<network::mojom::URLLoaderFactory> remote_url_loader_factory_;
  std::unique_ptr<GoogleApiTranslationDispatcher> translation_dispatcher_;
};

TEST_F(GoogleApiTranslationDispatcherTest, GetTranslationSuccess) {
  base::RunLoop translate_callback_run_loop;
  base::RunLoop wait_for_translation_request;
  base::MockCallback<TranslateEventCallback> translate_callback;
  std::string expected_translation = "hello world";

  EXPECT_CALL(translate_callback, Run(Eq(base::ok(expected_translation))))
      .WillOnce(testing::InvokeWithoutArgs(&translate_callback_run_loop,
                                           &base::RunLoop::Quit));

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        // Stop waiting as soon as the factory sees a request.
        wait_for_translation_request.Quit();
      }));

  GetTranslation("hola mundo", "es", "en", translate_callback);
  wait_for_translation_request.Run();

  // Verify the request.
  network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingRequest();
  ASSERT_TRUE(pending_request);
  EXPECT_EQ(pending_request->request.method, "POST");
  EXPECT_EQ(pending_request->request.url,
            "https://translation.googleapis.com/language/translate/"
            "v2?key=test_api_key");

  // Respond with a successful translation.
  std::string response_body = R"({
      "data": {
        "translations": [
          { "translatedText": "hello world" }
        ]
      }
    })";
  SimulateSuccessfulResponse(response_body);

  translate_callback_run_loop.Run();
}

TEST_F(GoogleApiTranslationDispatcherTest, GetTranslationNetworkError) {
  base::RunLoop translate_callback_run_loop;
  base::RunLoop wait_for_translation_request;
  base::MockCallback<TranslateEventCallback> translate_callback;

  EXPECT_CALL(translate_callback,
              Run(testing::Property(&TranslateEvent::has_value, false)))
      .WillOnce(testing::InvokeWithoutArgs(&translate_callback_run_loop,
                                           &base::RunLoop::Quit));

  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        // Stop waiting as soon as the factory sees a request.
        wait_for_translation_request.Quit();
      }));
  GetTranslation("hola mundo", "es", "en", translate_callback);
  wait_for_translation_request.Run();

  network::TestURLLoaderFactory::PendingRequest* pending_request =
      GetPendingRequest();
  ASSERT_TRUE(pending_request);

  // Simulate a network error.
  SimulateResponseForPendingRequest(
      network::URLLoaderCompletionStatus(net::ERR_FAILED),
      network::mojom::URLResponseHead::New(), std::string());

  translate_callback_run_loop.Run();
}

}  // namespace captions
