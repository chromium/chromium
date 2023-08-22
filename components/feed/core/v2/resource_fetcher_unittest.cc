// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/resource_fetcher.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/feed/core/v2/public/types.h"
#include "components/feed/core/v2/test/callback_receiver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::HasSubstr;

namespace feed {
namespace {

std::string GetStringFromDataElements(
    const std::vector<network::DataElement>* data_elements) {
  std::string result;
  for (const network::DataElement& element : *data_elements) {
    DCHECK_EQ(element.type(), network::DataElement::Tag::kBytes);
    // Provide the length of the bytes explicitly, not to rely on the null
    // termination.
    const auto piece = element.As<network::DataElementBytes>().AsStringPiece();
    result.append(piece);
  }
  return result;
}

class ResourceFetcherTest : public testing::Test {
 public:
  ResourceFetcherTest() = default;
  ResourceFetcherTest(ResourceFetcherTest&) = delete;
  ResourceFetcherTest& operator=(const ResourceFetcherTest&) = delete;
  ~ResourceFetcherTest() override = default;

  void SetUp() override {
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_factory_);
    resource_fetcher_ =
        std::make_unique<ResourceFetcher>(shared_url_loader_factory_);
  }

  ResourceFetcher* resource_fetcher() { return resource_fetcher_.get(); }

  network::ResourceRequest Respond(
      const std::string& additional_response_headers,
      const std::string& response_string,
      int net_status_code,
      int request_index = 0) {
    network::URLLoaderCompletionStatus status;

    task_environment_.RunUntilIdle();
    network::TestURLLoaderFactory::PendingRequest* pending_request =
        test_factory_.GetPendingRequest(request_index);
    CHECK(pending_request);
    network::ResourceRequest resource_request = pending_request->request;
    auto head = network::mojom::URLResponseHead::New();
    if (net_status_code >= 0) {
      std::string headers_str =
          "HTTP/1.1 " + base::NumberToString(net_status_code);
      if (!additional_response_headers.empty()) {
        headers_str += "\n";
        headers_str += additional_response_headers;
      }
      headers_str + "\n";
      head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
          net::HttpUtil::AssembleRawHeaders(headers_str));
      status.decoded_body_length = response_string.length();
    } else {
      status.error_code = net_status_code;
    }

    test_factory_.AddResponse(pending_request->request.url, std::move(head),
                              response_string, status);
    task_environment_.FastForwardUntilNoTasksRemain();
    return resource_request;
  }

 private:
  std::unique_ptr<ResourceFetcher> resource_fetcher_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ResourceFetcherTest, GetRequestWithEmptyResponse) {
  base::HistogramTester histograms;
  CallbackReceiver<NetworkResponse> receiver;
  resource_fetcher()->Fetch(GURL("https://example.com"),
                            net::HttpRequestHeaders::kGetMethod, {}, "",
                            receiver.Bind());
  network::ResourceRequest resource_request = Respond("", "", net::HTTP_OK);

  EXPECT_EQ(GURL("https://example.com"), resource_request.url);
  EXPECT_EQ("GET", resource_request.method);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_EQ(net::HTTP_OK, receiver.GetResult()->status_code);
  EXPECT_EQ("", receiver.GetResult()->response_bytes);

  EXPECT_EQ(
      std::vector<base::Bucket>({{net::HTTP_OK, 1}}),
      histograms.GetAllSamples("ContentSuggestions.Feed.ResourceFetchStatus"));
}

TEST_F(ResourceFetcherTest, GetRequestWithCustomResponseHeader) {
  base::HistogramTester histograms;
  CallbackReceiver<NetworkResponse> receiver;
  resource_fetcher()->Fetch(GURL("https://example.com"),
                            net::HttpRequestHeaders::kGetMethod, {}, "",
                            receiver.Bind());
  Respond("Header1: Value1\nHeader2: Value2\n", "", net::HTTP_OK);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_EQ(net::HTTP_OK, receiver.GetResult()->status_code);
  std::vector<std::string> expected_response_headers = {"Header1", "Value1",
                                                        "Header2", "Value2"};
  EXPECT_THAT(receiver.GetResult()->response_header_names_and_values,
              expected_response_headers);
}

TEST_F(ResourceFetcherTest, GetRequestWithNetError) {
  base::HistogramTester histograms;
  CallbackReceiver<NetworkResponse> receiver;
  resource_fetcher()->Fetch(GURL("https://example.com"),
                            net::HttpRequestHeaders::kGetMethod, {}, "",
                            receiver.Bind());
  Respond("", "", net::ERR_ACCESS_DENIED);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_EQ(net::ERR_ACCESS_DENIED, receiver.GetResult()->status_code);

  EXPECT_EQ(
      std::vector<base::Bucket>({{net::ERR_ACCESS_DENIED, 1}}),
      histograms.GetAllSamples("ContentSuggestions.Feed.ResourceFetchStatus"));
}

TEST_F(ResourceFetcherTest, GetRequestWithHttpError) {
  base::HistogramTester histograms;
  CallbackReceiver<NetworkResponse> receiver;
  resource_fetcher()->Fetch(GURL("https://example.com"),
                            net::HttpRequestHeaders::kGetMethod, {}, "",
                            receiver.Bind());
  Respond("", "", 404);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_EQ(404, receiver.GetResult()->status_code);

  EXPECT_EQ(
      std::vector<base::Bucket>({{404, 1}}),
      histograms.GetAllSamples("ContentSuggestions.Feed.ResourceFetchStatus"));
}

TEST_F(ResourceFetcherTest, GetRequestWithPostBody) {
  CallbackReceiver<NetworkResponse> receiver;
  resource_fetcher()->Fetch(GURL("https://example.com"),
                            net::HttpRequestHeaders::kGetMethod, {},
                            "post body", receiver.Bind());

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, receiver.GetResult()->status_code);
}

TEST_F(ResourceFetcherTest, GetRequestWithHeaders) {
  CallbackReceiver<NetworkResponse> receiver;
  resource_fetcher()->Fetch(
      GURL("https://example.com"), net::HttpRequestHeaders::kGetMethod,
      {"Header1", "Value1", "Header2", "Value2"}, "", receiver.Bind());
  network::ResourceRequest resource_request = Respond("", "", net::HTTP_OK);

  EXPECT_EQ(GURL("https://example.com"), resource_request.url);
  EXPECT_EQ("GET", resource_request.method);

  std::string value1;
  EXPECT_TRUE(resource_request.headers.GetHeader("Header1", &value1));
  EXPECT_EQ("Value1", value1);
  std::string value2;
  EXPECT_TRUE(resource_request.headers.GetHeader("Header2", &value2));
  EXPECT_EQ("Value2", value2);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_EQ(net::HTTP_OK, receiver.GetResult()->status_code);
}

TEST_F(ResourceFetcherTest, GetRequestWithIncompleteHeaders) {
  CallbackReceiver<NetworkResponse> receiver;
  resource_fetcher()->Fetch(GURL("https://example.com"),
                            net::HttpRequestHeaders::kGetMethod, {"Header1"},
                            "", receiver.Bind());

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_EQ(net::ERR_INVALID_ARGUMENT, receiver.GetResult()->status_code);
}

TEST_F(ResourceFetcherTest, PostRequest) {
  CallbackReceiver<NetworkResponse> receiver;
  resource_fetcher()->Fetch(GURL("https://example.com"),
                            net::HttpRequestHeaders::kPostMethod, {},
                            "post body", receiver.Bind());
  network::ResourceRequest resource_request =
      Respond("", "example_response", net::HTTP_OK);

  EXPECT_EQ(GURL("https://example.com"), resource_request.url);
  EXPECT_EQ("POST", resource_request.method);
  EXPECT_EQ("post body", GetStringFromDataElements(
                             resource_request.request_body->elements()));

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_EQ(net::HTTP_OK, receiver.GetResult()->status_code);
  EXPECT_THAT(receiver.GetResult()->response_bytes,
              HasSubstr("example_response"));
}

TEST_F(ResourceFetcherTest, UnsupportedHttpMethods) {
  std::string methods[] = {"CONNECT", "DELETE", "OPTIONS", "PATCH",
                           "PUT",     "TRACE",  "TRACK",   "ZZZ"};

  for (auto& method : methods) {
    CallbackReceiver<NetworkResponse> receiver;
    resource_fetcher()->Fetch(GURL("https://example.com"), method, {}, "",
                              receiver.Bind());
    ASSERT_TRUE(receiver.GetResult());
    EXPECT_EQ(net::ERR_METHOD_NOT_SUPPORTED, receiver.GetResult()->status_code);
  }
}

TEST_F(ResourceFetcherTest, GetRequestWithResponse) {
  CallbackReceiver<NetworkResponse> receiver;
  resource_fetcher()->Fetch(GURL("https://example.com"),
                            net::HttpRequestHeaders::kGetMethod, {}, "",
                            receiver.Bind());
  Respond("", "example_response", net::HTTP_OK);

  ASSERT_TRUE(receiver.GetResult());
  EXPECT_THAT(receiver.GetResult()->response_bytes,
              HasSubstr("example_response"));
  EXPECT_EQ(net::HTTP_OK, receiver.GetResult()->status_code);
}

TEST_F(ResourceFetcherTest, SequentialRequests) {
  CallbackReceiver<NetworkResponse> receiver1;
  resource_fetcher()->Fetch(GURL("https://example1.com"),
                            net::HttpRequestHeaders::kGetMethod, {}, "",
                            receiver1.Bind());
  Respond("", "example1_response", net::HTTP_OK);

  CallbackReceiver<NetworkResponse> receiver2;
  resource_fetcher()->Fetch(GURL("https://example2.com"),
                            net::HttpRequestHeaders::kGetMethod, {}, "",
                            receiver2.Bind());
  Respond("", "example2_response", net::HTTP_OK);

  ASSERT_TRUE(receiver1.GetResult());
  EXPECT_THAT(receiver1.GetResult()->response_bytes,
              HasSubstr("example1_response"));
  ASSERT_TRUE(receiver2.GetResult());
  EXPECT_THAT(receiver2.GetResult()->response_bytes,
              HasSubstr("example2_response"));
}

TEST_F(ResourceFetcherTest, ParallelRequests) {
  CallbackReceiver<NetworkResponse> receiver1;
  resource_fetcher()->Fetch(GURL("https://example1.com"),
                            net::HttpRequestHeaders::kGetMethod, {}, "",
                            receiver1.Bind());
  CallbackReceiver<NetworkResponse> receiver2;
  resource_fetcher()->Fetch(GURL("https://example2.com"),
                            net::HttpRequestHeaders::kGetMethod, {}, "",
                            receiver2.Bind());

  Respond("", "example2_response", net::HTTP_OK, 1);
  ASSERT_TRUE(receiver2.GetResult());
  EXPECT_THAT(receiver2.GetResult()->response_bytes,
              HasSubstr("example2_response"));

  Respond("", "example1_response", net::HTTP_OK, 0);
  ASSERT_TRUE(receiver1.GetResult());
  EXPECT_THAT(receiver1.GetResult()->response_bytes,
              HasSubstr("example1_response"));
}

}  // namespace
}  // namespace feed
