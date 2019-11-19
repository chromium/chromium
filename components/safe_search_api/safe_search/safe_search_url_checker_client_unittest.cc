// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_search_api/safe_search/safe_search_url_checker_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace safe_search_api {

namespace {

constexpr char kSafeSearchApiUrl[] =
    "https://safesearch.googleapis.com/v1:classify";

std::string BuildResponse(bool is_porn) {
  base::DictionaryValue dict;
  auto classification_dict = std::make_unique<base::DictionaryValue>();
  if (is_porn)
    classification_dict->SetBoolean("pornography", is_porn);
  auto classifications_list = std::make_unique<base::ListValue>();
  classifications_list->Append(std::move(classification_dict));
  dict.SetWithoutPathExpansion("classifications",
                               std::move(classifications_list));
  std::string result;
  base::JSONWriter::Write(dict, &result);
  return result;
}

const char* kURLs[] = {
    "http://www.randomsite1.com", "http://www.randomsite2.com",
    "http://www.randomsite3.com", "http://www.randomsite4.com",
    "http://www.randomsite5.com", "http://www.randomsite6.com",
    "http://www.randomsite7.com", "http://www.randomsite8.com",
    "http://www.randomsite9.com",
};

}  // namespace

class SafeSearchURLCheckerClientTest : public testing::Test {
 public:
  SafeSearchURLCheckerClientTest()
      : next_url_(0),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    checker_ = std::make_unique<SafeSearchURLCheckerClient>(
        test_shared_loader_factory_, TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  MOCK_METHOD2(OnCheckDone,
               void(const GURL& url, ClientClassification classification));

 protected:
  GURL GetNewURL() {
    CHECK(next_url_ < base::size(kURLs));
    return GURL(kURLs[next_url_++]);
  }

  void CheckURL(const GURL& url) {
    checker_->CheckURL(
        url, base::BindOnce(&SafeSearchURLCheckerClientTest::OnCheckDone,
                            base::Unretained(this)));
  }

  void WaitForResponse() { base::RunLoop().RunUntilIdle(); }

  void SetUpFailedResponse() { SetUpResponse(net::ERR_ABORTED, std::string()); }

  void SetUpResponse(net::Error error, const std::string& response) {
    network::URLLoaderCompletionStatus status(error);
    status.decoded_body_length = response.size();
    test_url_loader_factory_.AddResponse(GURL(kSafeSearchApiUrl),
                                         network::mojom::URLResponseHead::New(),
                                         response, status);
  }

  // This method should only be used for Classification kAllowed and kRestricted
  void SetUpValidResponse(ClientClassification classification) {
    bool is_porn = classification == ClientClassification::kRestricted;
    SetUpResponse(net::OK, BuildResponse(is_porn));
  }

  void SendValidResponse(const GURL& url, ClientClassification classification) {
    SetUpValidResponse(classification);
    CheckURL(url);
    WaitForResponse();
  }

  void SendFailedResponse(const GURL& url) {
    SetUpFailedResponse();
    CheckURL(url);
    WaitForResponse();
  }

  size_t next_url_;
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<SafeSearchURLCheckerClient> checker_;
};

TEST_F(SafeSearchURLCheckerClientTest, Simple) {
  {
    GURL url(GetNewURL());
    EXPECT_CALL(*this, OnCheckDone(url, ClientClassification::kAllowed));
    SendValidResponse(url, ClientClassification::kAllowed);
  }
  {
    GURL url(GetNewURL());
    EXPECT_CALL(*this, OnCheckDone(url, ClientClassification::kRestricted));
    SendValidResponse(url, ClientClassification::kRestricted);
  }
  {
    GURL url(GetNewURL());
    EXPECT_CALL(*this, OnCheckDone(url, ClientClassification::kUnknown));
    SendFailedResponse(url);
  }
}

}  // namespace safe_search_api
