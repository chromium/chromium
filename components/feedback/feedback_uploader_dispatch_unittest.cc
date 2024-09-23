// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_uploader.h"

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/feedback/feedback_common.h"
#include "components/feedback/feedback_constants.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_ids_provider.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feedback {

namespace {

constexpr base::TimeDelta kTestRetryDelay = base::Milliseconds(1);

constexpr char kFeedbackPostUrl[] =
    "https://www.google.com/tools/feedback/chrome/__submit";

void QueueReport(FeedbackUploader* uploader,
                 const std::string& report_data,
                 bool has_email = true,
                 int product_id = 0) {
  uploader->QueueReport(std::make_unique<std::string>(report_data), has_email,
                        product_id);
}

class TestFeedbackUploader final : public FeedbackUploader {
 public:
  TestFeedbackUploader(
      bool is_off_the_record,
      const base::FilePath& state_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : FeedbackUploader(is_off_the_record,
                         state_path,
                         std::move(url_loader_factory)) {}
  TestFeedbackUploader(const TestFeedbackUploader&) = delete;
  TestFeedbackUploader& operator=(const TestFeedbackUploader&) = delete;

  ~TestFeedbackUploader() override = default;

  base::WeakPtr<FeedbackUploader> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestFeedbackUploader> weak_ptr_factory_{this};
};

// Stand-in for the FeedbackUploaderChrome class that adds a fake bearer token
// to the request.
class MockFeedbackUploaderChrome final : public FeedbackUploader {
 public:
  MockFeedbackUploaderChrome(
      bool is_off_the_record,
      const base::FilePath& state_path,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : FeedbackUploader(is_off_the_record, state_path, url_loader_factory) {}

  void AppendExtraHeadersToUploadRequest(
      network::ResourceRequest* resource_request) override {
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                        "Bearer abcdefg");
  }

  base::WeakPtr<FeedbackUploader> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockFeedbackUploaderChrome> weak_ptr_factory_{this};
};

}  // namespace

class FeedbackUploaderDispatchTest : public ::testing::Test {
 protected:
  FeedbackUploaderDispatchTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  FeedbackUploaderDispatchTest(const FeedbackUploaderDispatchTest&) = delete;
  FeedbackUploaderDispatchTest& operator=(const FeedbackUploaderDispatchTest&) =
      delete;

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
    base::FieldTrialList::CreateFieldTrial(trial_name, group_name)->Activate();
  }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory() {
    return shared_url_loader_factory_;
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  const base::FilePath& state_path() { return scoped_temp_dir_.GetPath(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_dir_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(FeedbackUploaderDispatchTest, VariationHeaders) {
  // Register a trial and variation id, so that there is data in variations
  // headers.
  CreateFieldTrialWithId("Test", "Group1", 123);

  TestFeedbackUploader uploader(/*is_off_the_record=*/false, state_path(),
                                shared_url_loader_factory());

  net::HttpRequestHeaders headers;
  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_TRUE(variations::HasVariationsHeader(request));
      }));

  QueueReport(&uploader, "test");
  base::RunLoop().RunUntilIdle();
}

TEST_F(FeedbackUploaderDispatchTest, VariationHeadersForProductID) {
  // Register a trial and variation id, so that there is data in variations
  // headers.
  CreateFieldTrialWithId("Test", "Group1", 123);

  TestFeedbackUploader uploader(/*is_off_the_record=*/false, state_path(),
                                shared_url_loader_factory());

  net::HttpRequestHeaders headers;
  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_TRUE(variations::HasVariationsHeader(request));
      }));

  QueueReport(&uploader, "test", /*has_email=*/false, /*product_id=*/208);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FeedbackUploaderDispatchTest, NoVariationHeadersForOrcaProductID) {
  // Register a trial and variation id, so that there is data in variations
  // headers.
  CreateFieldTrialWithId("Test", "Group1", 123);

  TestFeedbackUploader uploader(/*is_off_the_record=*/false, state_path(),
                                shared_url_loader_factory());

  net::HttpRequestHeaders headers;
  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_FALSE(variations::HasVariationsHeader(request));
      }));

  QueueReport(&uploader, "test", /*has_email=*/false,
              /*product_id=*/feedback::kOrcaFeedbackProductId);
  base::RunLoop().RunUntilIdle();
}

// Test that the bearer token is present if there is an email address present in
// the report.
TEST_F(FeedbackUploaderDispatchTest, BearerTokenWithEmail) {
  MockFeedbackUploaderChrome uploader(/*is_off_the_record=*/false, state_path(),
                                      shared_url_loader_factory());

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_TRUE(request.headers.HasHeader("Authorization"));
      }));

  QueueReport(&uploader, "test", /*has_email=*/true);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FeedbackUploaderDispatchTest, BearerTokenNoEmail) {
  MockFeedbackUploaderChrome uploader(/*is_off_the_record=*/false, state_path(),
                                      shared_url_loader_factory());

  test_url_loader_factory()->SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        EXPECT_FALSE(request.headers.HasHeader("Authorization"));
      }));

  QueueReport(&uploader, "test", /*has_email=*/false);
  base::RunLoop().RunUntilIdle();
}

TEST_F(FeedbackUploaderDispatchTest, 204Response) {
  FeedbackUploader::SetMinimumRetryDelayForTesting(kTestRetryDelay);
  TestFeedbackUploader uploader(/*is_off_the_record=*/false, state_path(),
                                shared_url_loader_factory());

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
  TestFeedbackUploader uploader(/*is_off_the_record=*/false, state_path(),
                                shared_url_loader_factory());

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
  TestFeedbackUploader uploader(/*is_off_the_record=*/false, state_path(),
                                shared_url_loader_factory());

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
