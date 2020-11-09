// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader.h"

#include <string>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/test/bind_test_util.h"
#include "components/feedback/feedback_uploader_factory.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_ids_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feedback {

namespace {

constexpr base::TimeDelta kTestRetryDelay =
    base::TimeDelta::FromMilliseconds(1);

constexpr char kFeedbackPostUrl[] =
    "https://www.google.com/tools/feedback/chrome/__submit";

void QueueReport(FeedbackUploader* uploader,
                 const std::string& report_data,
                 bool has_email = true) {
  uploader->QueueReport(std::make_unique<std::string>(report_data), has_email);
}

// Stand-in for the FeedbackUploaderChrome class that adds a fake bearer token
// to the request.
class MockFeedbackUploaderChrome : public FeedbackUploader {
 public:
  MockFeedbackUploaderChrome(
      content::BrowserContext* context,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : FeedbackUploader(context, task_runner) {}

  void AppendExtraHeadersToUploadRequest(
      network::ResourceRequest* resource_request) override {
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                        "Bearer abcdefg");
  }
};

}  // namespace

class FeedbackUploaderDispatchTest : public ::testing::Test {
 protected:
  FeedbackUploaderDispatchTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  ~FeedbackUploaderDispatchTest() override {
    // Clean up registered ids.
    variations::testing::ClearAllVariationIDs();
  }

  // Registers a field trial with the specified name and group and an associated
  // google web property variation id.
  void CreateFieldTrialWithId(const std::string& trial_name,
                              const std::string& group_name,
                              int variation_id) {
    variations::AssociateGoogleVariationID(
        variations::GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, trial_name, group_name,
        static_cast<variations::VariationID>(variation_id));
    base::FieldTrialList::CreateFieldTrial(trial_name, group_name)->group();
  }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory() {
    return shared_url_loader_factory_;
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  content::BrowserContext* context() { return &context_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext context_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploaderDispatchTest);
};

TEST_F(FeedbackUploaderDispatchTest, VariationHeaders) {
  // Register a trial and variation id, so that there is data in variations
  // headers. Also, the variations header provider may have been registered to
  // observe some other field trial list, so reset it.
  variations::VariationsIdsProvider::GetInstance()->ResetForTesting();
  CreateFieldTrialWithId("Test", "Group1", 123);

  FeedbackUploader uploader(
      context(), FeedbackUploaderFactory::CreateUploaderTaskRunner());
  uploader.set_url_loader_factory_for_test(shared_url_loader_factory());

  net::HttpRequestHeaders headers;
  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_TRUE(variations::HasVariationsHeader(request));
      }));

  QueueReport(&uploader, "test");
  base::RunLoop().RunUntilIdle();

  variations::VariationsIdsProvider::GetInstance()->ResetForTesting();
}

// Test that the bearer token is present if there is an email address present in
// the report.
TEST_F(FeedbackUploaderDispatchTest, BearerTokenWithEmail) {
  MockFeedbackUploaderChrome uploader(
      context(), FeedbackUploaderFactory::CreateUploaderTaskRunner());
  uploader.set_url_loader_factory_for_test(shared_url_loader_factory());

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_TRUE(request.headers.HasHeader("Authorization"));
      }));

  QueueReport(&uploader, "test", /*has_email=*/true);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FeedbackUploaderDispatchTest, BearerTokenNoEmail) {
  MockFeedbackUploaderChrome uploader(
      context(), FeedbackUploaderFactory::CreateUploaderTaskRunner());
  uploader.set_url_loader_factory_for_test(shared_url_loader_factory());

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_FALSE(request.headers.HasHeader("Authorization"));
      }));

  QueueReport(&uploader, "test", /*has_email=*/false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FeedbackUploaderDispatchTest, 204Response) {
  FeedbackUploader::SetMinimumRetryDelayForTesting(kTestRetryDelay);
  FeedbackUploader uploader(
      context(), FeedbackUploaderFactory::CreateUploaderTaskRunner());
  uploader.set_url_loader_factory_for_test(shared_url_loader_factory());

  EXPECT_EQ(kTestRetryDelay, uploader.retry_delay());
  // Successful reports should not introduce any retries, and should not
  // increase the backoff delay.
  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 204 No Content\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  network::URLLoaderCompletionStatus status;
  test_url_loader_factory()->AddResponse(GURL(kFeedbackPostUrl),
                                         std::move(head), "", status);
  QueueReport(&uploader, "Successful report");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTestRetryDelay, uploader.retry_delay());
  EXPECT_TRUE(uploader.QueueEmpty());
}

TEST_F(FeedbackUploaderDispatchTest, 400Response) {
  FeedbackUploader::SetMinimumRetryDelayForTesting(kTestRetryDelay);
  FeedbackUploader uploader(
      context(), FeedbackUploaderFactory::CreateUploaderTaskRunner());
  uploader.set_url_loader_factory_for_test(shared_url_loader_factory());

  EXPECT_EQ(kTestRetryDelay, uploader.retry_delay());
  // Failed reports due to client errors are not retried. No backoff delay
  // should be doubled.
  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 400 Bad Request\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  network::URLLoaderCompletionStatus status;
  test_url_loader_factory()->AddResponse(GURL(kFeedbackPostUrl),
                                         std::move(head), "", status);
  QueueReport(&uploader, "Client error failed report");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(kTestRetryDelay, uploader.retry_delay());
  EXPECT_TRUE(uploader.QueueEmpty());
}

TEST_F(FeedbackUploaderDispatchTest, 500Response) {
  FeedbackUploader::SetMinimumRetryDelayForTesting(kTestRetryDelay);
  FeedbackUploader uploader(
      context(), FeedbackUploaderFactory::CreateUploaderTaskRunner());
  uploader.set_url_loader_factory_for_test(shared_url_loader_factory());

  EXPECT_EQ(kTestRetryDelay, uploader.retry_delay());
  // Failed reports due to server errors are retried.
  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 500 Server Error\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  network::URLLoaderCompletionStatus status;
  test_url_loader_factory()->AddResponse(GURL(kFeedbackPostUrl),
                                         std::move(head), "", status);
  QueueReport(&uploader, "Server error failed report");
  base::RunLoop().RunUntilIdle();

  EXPECT_LT(kTestRetryDelay, uploader.retry_delay());
  EXPECT_FALSE(uploader.QueueEmpty());
}

}  // namespace feedback
