// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/server/bloom_server_proxy_impl.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/optional.h"
#include "chromeos/components/bloom/server/bloom_url_loader.h"
#include "chromeos/services/assistant/public/shared/constants.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace chromeos {
namespace bloom {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::SaveArgPointee;

using assistant::kBloomCreateImagePath;
using assistant::kBloomOcrImagePath;
using assistant::kBloomSearchProblemPath;

// We can't simply pass |base::nullopt| into an EXPECT_CALL call,
// instead it must be fully typed to |base::Optional<std::string>|.
// So this convenience wrapper hides those details.
#define EXPECT_CALL_WITH_NULLOPT(mock) \
  EXPECT_CALL(mock, Call(base::Optional<std::string>(base::nullopt)))

std::string Quote(const std::string& data) {
  return "\"" + data + "\"";
}

std::string ToJSON(const std::string& name, const std::string& value) {
  return "{" + Quote(name) + ":" + Quote(value) + "}";
}

std::string ImageToJSON(const std::string& name, const gfx::Image& value) {
  auto bytes = value.As1xPNGBytes();
  return ToJSON(
      name, base::Base64Encode({bytes->front_as<uint8_t>(), bytes->size()}));
}

// Mock for |BloomServerProxy::Callback|.
class CallbackMock {
 public:
  MOCK_METHOD(void, Call, (base::Optional<std::string>));

  auto BindOnce() {
    return base::BindOnce(&CallbackMock::Call, base::Unretained(this));
  }
};

class BloomURLLoaderMock : public BloomURLLoader {
 public:
  BloomURLLoaderMock() {
    ON_CALL(*this, SendPostRequest)
        .WillByDefault([this](const GURL&, const std::string&, std::string&&,
                              const std::string&, Callback callback) {
          this->post_callback_ = std::move(callback);
        });

    ON_CALL(*this, SendGetRequest)
        .WillByDefault(
            [this](const GURL&, const std::string&, Callback callback) {
              this->get_callback_ = std::move(callback);
            });
  }

  MOCK_METHOD(void,
              SendPostRequest,
              (const GURL& url,
               const std::string& access_token,
               std::string&& body,
               const std::string& mime_type,
               Callback callback));
  MOCK_METHOD(void,
              SendGetRequest,
              (const GURL& url,
               const std::string& access_token,
               Callback callback));

  void SendPostServerReply(base::Optional<std::string> reply) {
    DCHECK(post_callback_)
        << "Asked to send a reply but haven't received a POST request";
    std::move(post_callback_).Run(std::move(reply));
  }

  void SendGetServerReply(base::Optional<std::string> reply) {
    DCHECK(get_callback_)
        << "Asked to send a reply but haven't received a GET request";
    std::move(get_callback_).Run(std::move(reply));
  }

 private:
  Callback post_callback_;
  Callback get_callback_;
};

}  // namespace

class BloomServerProxyImplTest : public ::testing::Test {
 protected:
  BloomURLLoaderMock& url_loader_mock() {
    return *static_cast<BloomURLLoaderMock*>(server_proxy_.url_loader());
  }

  BloomServerProxyImpl& server_proxy() { return server_proxy_; }

  GURL GetUrlWithPath(const std::string& path) {
    return GURL(std::string(assistant::kBloomServiceUrl) + path);
  }

  auto any_screenshot() { return gfx::test::CreateImage(5, 5); }

  auto any_callback() {
    return base::BindOnce([](base::Optional<std::string>) {});
  }

  // Add mock expectations for all server calls.
  void ExpectServerCalls() {
    EXPECT_CALL(url_loader_mock(), SendPostRequest).Times(AnyNumber());
    EXPECT_CALL(url_loader_mock(), SendGetRequest).Times(AnyNumber());
  }

  void RespondToUploadImageCall(base::Optional<std::string> json_response =
                                    ToJSON("imageId", "default-image-id")) {
    url_loader_mock().SendPostServerReply(json_response);
  }

  void RespondToOcrImageCall(base::Optional<std::string> json_response =
                                 ToJSON("metadataBlob",
                                        "default-metadata-blob")) {
    url_loader_mock().SendGetServerReply(json_response);
  }

  void RespondToProblemSearchCall(
      base::Optional<std::string> server_response = "default-server-response") {
    url_loader_mock().SendGetServerReply(server_response);
  }

 private:
  BloomServerProxyImpl server_proxy_{std::make_unique<BloomURLLoaderMock>()};
};

TEST_F(BloomServerProxyImplTest, ShouldUploadScreenshot) {
  gfx::Image screenshot = gfx::test::CreateImage(10, 20);

  std::string expected_request = ImageToJSON("raw_data", screenshot);

  EXPECT_CALL(
      url_loader_mock(),
      SendPostRequest(GetUrlWithPath(kBloomCreateImagePath), "access_token",
                      std::move(expected_request), "application/json", _));

  server_proxy().AnalyzeProblem("access_token", std::move(screenshot),
                                any_callback());
}

TEST_F(BloomServerProxyImplTest, ShouldSendOcrRequestAfterUploadingScreenshot) {
  const std::string image_id = "the-image-id";

  // First we expect a call to upload the image.
  EXPECT_CALL(url_loader_mock(), SendPostRequest);

  // Next we expect a call to perform OCR.
  EXPECT_CALL(url_loader_mock(),
              SendGetRequest(GetUrlWithPath(kBloomOcrImagePath + image_id),
                             "access_token", _));

  server_proxy().AnalyzeProblem("access_token", any_screenshot(),
                                any_callback());

  RespondToUploadImageCall(ToJSON("imageId", "the-image-id"));
}

TEST_F(BloomServerProxyImplTest,
       ShouldSendSearchProblemRequestAfterOcrRequest) {
  const std::string metadata_blob = "the-metadata-blob";

  // First we expect a call to upload the image.
  EXPECT_CALL(url_loader_mock(), SendPostRequest);
  // Next we expect a call to perform OCR.
  EXPECT_CALL(url_loader_mock(), SendGetRequest);

  // Finally we expect a call to search the problem
  EXPECT_CALL(
      url_loader_mock(),
      SendGetRequest(GetUrlWithPath(kBloomSearchProblemPath + metadata_blob),
                     "access_token", _));

  server_proxy().AnalyzeProblem("access_token", any_screenshot(),
                                any_callback());

  RespondToUploadImageCall();
  RespondToOcrImageCall(ToJSON("metadataBlob", metadata_blob));
  RespondToProblemSearchCall();
}

TEST_F(BloomServerProxyImplTest, ShouldSendServerResponseToTheCallback) {
  const base::Optional<std::string> server_response = "the-server-response";

  CallbackMock callback;

  ExpectServerCalls();

  EXPECT_CALL(callback, Call(server_response));

  server_proxy().AnalyzeProblem("access_token", any_screenshot(),
                                callback.BindOnce());

  RespondToUploadImageCall();
  RespondToOcrImageCall();
  RespondToProblemSearchCall(server_response);
}

TEST_F(BloomServerProxyImplTest,
       ShouldSendNulloptToCallbackIfCreateImageFails) {
  CallbackMock callback;
  ExpectServerCalls();

  EXPECT_CALL_WITH_NULLOPT(callback);

  server_proxy().AnalyzeProblem("access_token", any_screenshot(),
                                callback.BindOnce());

  RespondToUploadImageCall(/*json_response=*/base::nullopt);
}

TEST_F(BloomServerProxyImplTest,
       ShouldSendNulloptToCallbackIfCreateImageReturnsInvalidJSON) {
  CallbackMock callback;
  ExpectServerCalls();

  EXPECT_CALL_WITH_NULLOPT(callback);

  server_proxy().AnalyzeProblem("access_token", any_screenshot(),
                                callback.BindOnce());

  RespondToUploadImageCall(/*json_response=*/"invalid-json");
}

TEST_F(BloomServerProxyImplTest,
       ShouldSendNulloptToCallbackIfCreateImageReturnsNoImageId) {
  CallbackMock callback;
  ExpectServerCalls();

  EXPECT_CALL_WITH_NULLOPT(callback);

  server_proxy().AnalyzeProblem("access_token", any_screenshot(),
                                callback.BindOnce());

  RespondToUploadImageCall(ToJSON("wrongJSONTag", "value"));
}

TEST_F(BloomServerProxyImplTest, ShouldSendNulloptToCallbackIfOcrImageFails) {
  CallbackMock callback;
  ExpectServerCalls();

  EXPECT_CALL_WITH_NULLOPT(callback);

  server_proxy().AnalyzeProblem("access_token", any_screenshot(),
                                callback.BindOnce());

  RespondToUploadImageCall();
  RespondToOcrImageCall(/*json_response=*/base::nullopt);
}

TEST_F(BloomServerProxyImplTest,
       ShouldSendNulloptToCallbackIfOcrImageReturnsInvalidJSON) {
  CallbackMock callback;
  ExpectServerCalls();

  EXPECT_CALL_WITH_NULLOPT(callback);

  server_proxy().AnalyzeProblem("access_token", any_screenshot(),
                                callback.BindOnce());

  RespondToUploadImageCall();
  RespondToOcrImageCall(/*json_response=*/base::nullopt);
}

TEST_F(BloomServerProxyImplTest,
       ShouldSendNulloptToCallbackIfOcrImageReturnsNoMetadataBlob) {
  CallbackMock callback;
  ExpectServerCalls();

  EXPECT_CALL_WITH_NULLOPT(callback);

  server_proxy().AnalyzeProblem("access_token", any_screenshot(),
                                callback.BindOnce());

  RespondToUploadImageCall();
  RespondToOcrImageCall(ToJSON("wrongJSONTag", "value"));
}

TEST_F(BloomServerProxyImplTest,
       ShouldSendNulloptToCallbackIfProblemSearchFails) {
  CallbackMock callback;
  ExpectServerCalls();

  EXPECT_CALL_WITH_NULLOPT(callback);

  server_proxy().AnalyzeProblem("access_token", any_screenshot(),
                                callback.BindOnce());

  RespondToUploadImageCall();
  RespondToOcrImageCall();
  RespondToProblemSearchCall(/*json_response=*/base::nullopt);
}

TEST_F(BloomServerProxyImplTest,
       ShouldSendNulloptToCallbackIfInteractionIsCancelled) {
  CallbackMock callback;
  ExpectServerCalls();

  EXPECT_CALL_WITH_NULLOPT(callback);

  server_proxy().AnalyzeProblem("access_token", any_screenshot(),
                                callback.BindOnce());

  // To cancel the interaction, we start a second interaction.
  server_proxy().AnalyzeProblem("access_token", any_screenshot(),
                                any_callback());
}

}  // namespace bloom
}  // namespace chromeos
