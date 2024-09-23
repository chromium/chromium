// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_impl.h"

#include <map>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_constants.h"
#include "chromeos/ash/components/enhanced_network_tts/enhanced_network_tts_test_utils.h"
#include "google_apis/google_api_keys.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::enhanced_network_tts {
namespace {

// A fake server that supports test URL loading.
class TestServerURLLoaderFactory {
 public:
  TestServerURLLoaderFactory()
      : shared_loader_factory_(loader_factory_.GetSafeWeakWrapper()) {}
  TestServerURLLoaderFactory(const TestServerURLLoaderFactory&) = delete;
  TestServerURLLoaderFactory& operator=(const TestServerURLLoaderFactory&) =
      delete;
  ~TestServerURLLoaderFactory() = default;

  const std::vector<network::TestURLLoaderFactory::PendingRequest>& requests() {
    return *loader_factory_.pending_requests();
  }

  // Expects that the earliest received request has the given URL, headers and
  // body, and replies with the given response.
  //
  // |expected_headers| is a map from header key string to either:
  //   a) a null optional, if the given header should not be present, or
  //   b) a non-null optional, if the given header should be present and match
  //      the optional value.
  //
  // Consumes the earliest received request (i.e. a subsequent call will apply
  // to the second-earliest received request and so on).
  void ExpectRequestAndSimulateResponse(
      const std::string& expected_url,
      const std::map<std::string, std::optional<std::string>>& expected_headers,
      const std::string& expected_body,
      const std::string& response,
      const net::HttpStatusCode response_code) {
    const std::vector<network::TestURLLoaderFactory::PendingRequest>&
        pending_requests = *loader_factory_.pending_requests();

    ASSERT_FALSE(pending_requests.empty());
    const network::ResourceRequest& request = pending_requests.front().request;

    // Assert that the earliest request is for the given URL.
    EXPECT_EQ(request.url, GURL(expected_url));

    // Expect that specified headers are accurate.
    for (const auto& kv : expected_headers) {
      EXPECT_EQ(request.headers.GetHeader(kv.first), kv.second);
    }

    // Extract request body.
    std::string actual_body;
    if (request.request_body) {
      const std::vector<network::DataElement>* const elements =
          request.request_body->elements();

      // We only support the simplest body structure.
      if (elements && elements->size() == 1u &&
          (*elements)[0].type() ==
              network::mojom::DataElementDataView::Tag::kBytes) {
        actual_body = std::string(
            (*elements)[0].As<network::DataElementBytes>().AsStringPiece());
      }
    }

    EXPECT_TRUE(AreRequestsEqual(actual_body, expected_body));

    // Guaranteed to match the first request based on URL.
    loader_factory_.SimulateResponseForPendingRequest(expected_url, response,
                                                      response_code);
  }

  scoped_refptr<network::SharedURLLoaderFactory> AsSharedURLLoaderFactory() {
    return shared_loader_factory_;
  }

 private:
  network::TestURLLoaderFactory loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_loader_factory_;
};

// Receives the result of a request and writes the result data into the given
// variables.
void UnpackResult(std::optional<mojom::TtsRequestError>* const error,
                  std::vector<uint8_t>* const audio_data,
                  std::vector<mojom::TimingInfo>* const timing_data,
                  mojom::TtsResponsePtr result) {
  if (result->which() == mojom::TtsResponse::Tag::kErrorCode) {
    *error = result->get_error_code();
  } else {
    // Copy audio data.
    for (const auto audio_data_pt : result->get_data()->audio)
      audio_data->push_back(audio_data_pt);

    // Copy timing data.
    for (const auto& timing_ptr : result->get_data()->time_info)
      timing_data->push_back(*timing_ptr);
  }
}

class TestAudioDataObserverImpl : public mojom::AudioDataObserver {
 public:
  TestAudioDataObserverImpl() = default;
  TestAudioDataObserverImpl(const TestAudioDataObserverImpl&) = delete;
  void operator=(const TestAudioDataObserverImpl&) = delete;
  ~TestAudioDataObserverImpl() override = default;

  // Binds a pending receiver.
  void BindReceiver(mojo::PendingReceiver<mojom::AudioDataObserver> receiver) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
  }

  // mojom::AudioDataObserver:
  void OnAudioDataReceived(mojom::TtsResponsePtr response) override {
    received_responses_.push_back(std::move(response));
  }

  mojom::TtsResponsePtr GetNexResponse() {
    mojom::TtsResponsePtr next_response =
        std::move(received_responses_.front());
    received_responses_.pop_front();
    return next_response;
  }

 private:
  std::list<mojom::TtsResponsePtr> received_responses_;

  mojo::Receiver<mojom::AudioDataObserver> receiver_{this};
};

}  // namespace

class EnhancedNetworkTtsImplTest : public testing::Test {
 protected:
  void SetUp() override {
    in_process_data_decoder_ =
        std::make_unique<data_decoder::test::InProcessDataDecoder>();
    enhanced_network_tts_impl_ = new EnhancedNetworkTtsImpl();
    enhanced_network_tts_impl_->BindReceiverAndURLFactory(
        remote_.BindNewPipeAndPassReceiver(),
        test_url_factory_.AsSharedURLLoaderFactory());
  }

  EnhancedNetworkTtsImpl& GetTestingInstance() {
    return *enhanced_network_tts_impl_;
  }

  TestAudioDataObserverImpl* GetTestingObserverPtr() { return &observer_; }

  raw_ptr<EnhancedNetworkTtsImpl> enhanced_network_tts_impl_;
  std::unique_ptr<data_decoder::test::InProcessDataDecoder>
      in_process_data_decoder_;
  base::test::TaskEnvironment test_task_env_;
  TestServerURLLoaderFactory test_url_factory_;
  mojo::Remote<mojom::EnhancedNetworkTts> remote_;
  TestAudioDataObserverImpl observer_;
};

TEST_F(EnhancedNetworkTtsImplTest, GetAudioDataSucceeds) {
  const std::string input_text = "Hi.";
  const float rate = 1.0;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, rate, std::nullopt, std::nullopt),
      base::BindOnce(
          [](TestAudioDataObserverImpl* observer,
             mojo::PendingReceiver<mojom::AudioDataObserver> pending_receiver) {
            observer->BindReceiver(std::move(pending_receiver));
          },
          GetTestingObserverPtr()));
  test_task_env_.RunUntilIdle();

  const std::map<std::string, std::optional<std::string>> expected_headers = {
      {kGoogApiKeyHeader, google_apis::GetReadAloudAPIKey()}};
  const std::string expected_body = CreateCorrectRequest(input_text, rate);
  // |expected_output| here is arbitrary, which is encoded into a fake response
  // sent by the fake server, |TestServerURLLoaderFactory|. In general, we
  // expect the real server sends the audio data back as a base64 encoded JSON
  // string.
  const std::vector<uint8_t> expected_output = {1, 2, 5};
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, expected_body,
      CreateServerResponse(expected_output), net::HTTP_OK);
  test_task_env_.RunUntilIdle();

  // We only get the data after the server's response. We simulate the response
  // in the code above.
  std::optional<mojom::TtsRequestError> error;
  std::vector<uint8_t> audio_data;
  std::vector<mojom::TimingInfo> timing_data;
  UnpackResult(&error, &audio_data, &timing_data,
               GetTestingObserverPtr()->GetNexResponse());
  EXPECT_EQ(audio_data, expected_output);
  // The timing data is hardcoded in |kTemplateResponse|.
  EXPECT_EQ(timing_data.size(), 2u);
  EXPECT_EQ(timing_data[0].text, "test1");
  EXPECT_EQ(timing_data[0].time_offset, "0.01s");
  EXPECT_EQ(timing_data[0].duration, "0.14s");
  EXPECT_EQ(timing_data[0].text_offset, 0u);
  EXPECT_EQ(timing_data[1].text, "test2");
  EXPECT_EQ(timing_data[1].time_offset, "0.16s");
  EXPECT_EQ(timing_data[1].duration, "0.17s");
  EXPECT_EQ(timing_data[1].text_offset, 6u);
}

TEST_F(EnhancedNetworkTtsImplTest, GetAudioDataIgnoresWhitespacesAtStart) {
  const std::string input_text = "    test1 test2";
  const std::string input_text_trimmed = "test1 test2";
  const float rate = 1.0;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, rate, std::nullopt, std::nullopt),
      base::BindOnce(
          [](TestAudioDataObserverImpl* observer,
             mojo::PendingReceiver<mojom::AudioDataObserver> pending_receiver) {
            observer->BindReceiver(std::move(pending_receiver));
          },
          GetTestingObserverPtr()));
  test_task_env_.RunUntilIdle();

  const std::map<std::string, std::optional<std::string>> expected_headers = {
      {kGoogApiKeyHeader, google_apis::GetReadAloudAPIKey()}};
  const std::string expected_body =
      CreateCorrectRequest(input_text_trimmed, rate);
  // |expected_output| here is arbitrary, which is encoded into a fake response
  // sent by the fake server, |TestServerURLLoaderFactory|. In general, we
  // expect the real server sends the audio data back as a base64 encoded JSON
  // string.
  const std::vector<uint8_t> expected_output = {1, 2, 5};
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, expected_body,
      CreateServerResponse(expected_output), net::HTTP_OK);
  test_task_env_.RunUntilIdle();

  // We only get the data after the server's response. We simulate the response
  // in the code above.
  std::optional<mojom::TtsRequestError> error;
  std::vector<uint8_t> audio_data;
  std::vector<mojom::TimingInfo> timing_data;
  UnpackResult(&error, &audio_data, &timing_data,
               GetTestingObserverPtr()->GetNexResponse());
  // The text offset will be compensated with whitespaces.
  EXPECT_EQ(timing_data[0].text, "test1");
  EXPECT_EQ(timing_data[0].text_offset, 4u);
  EXPECT_EQ(timing_data[1].text, "test2");
  EXPECT_EQ(timing_data[1].text_offset, 10u);
}

TEST_F(EnhancedNetworkTtsImplTest, GetAudioDataSucceedsWithFasterRate) {
  const std::string input_text = "Rate will be capped to kMaxRate";
  const float rate = kMaxRate + 1.0f;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, rate, std::nullopt, std::nullopt),
      base::BindOnce(
          [](TestAudioDataObserverImpl* observer,
             mojo::PendingReceiver<mojom::AudioDataObserver> pending_receiver) {
            observer->BindReceiver(std::move(pending_receiver));
          },
          GetTestingObserverPtr()));
  test_task_env_.RunUntilIdle();

  const std::map<std::string, std::optional<std::string>> expected_headers = {
      {kGoogApiKeyHeader, google_apis::GetReadAloudAPIKey()}};
  const std::string expected_body = CreateCorrectRequest(input_text, kMaxRate);
  // |expected_output| here is arbitrary, which is encoded into a fake response
  // sent by the fake server, |TestServerURLLoaderFactory|. In general, we
  // expect the real server sends the audio data back as a base64 encoded JSON
  // string.
  const std::vector<uint8_t> expected_output = {1, 2, 5};
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, expected_body,
      CreateServerResponse(expected_output), net::HTTP_OK);
  test_task_env_.RunUntilIdle();

  std::optional<mojom::TtsRequestError> error;
  std::vector<uint8_t> audio_data;
  std::vector<mojom::TimingInfo> timing_data;
  UnpackResult(&error, &audio_data, &timing_data,
               GetTestingObserverPtr()->GetNexResponse());
  // We only get the data after the server's response. We simulate the response
  // in the code above.
  EXPECT_EQ(audio_data, expected_output);
}

TEST_F(EnhancedNetworkTtsImplTest, GetAudioDataSucceedsWithSlowerRate) {
  const std::string input_text = "Rate will be floored to kMinRate";
  const float rate = kMinRate - 0.1f;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, rate, std::nullopt, std::nullopt),
      base::BindOnce(
          [](TestAudioDataObserverImpl* observer,
             mojo::PendingReceiver<mojom::AudioDataObserver> pending_receiver) {
            observer->BindReceiver(std::move(pending_receiver));
          },
          GetTestingObserverPtr()));
  test_task_env_.RunUntilIdle();

  const std::map<std::string, std::optional<std::string>> expected_headers = {
      {kGoogApiKeyHeader, google_apis::GetReadAloudAPIKey()}};
  const std::string expected_body = CreateCorrectRequest(input_text, kMinRate);
  // |expected_output| here is arbitrary, which is encoded into a fake response
  // sent by the fake server, |TestServerURLLoaderFactory|. In general, we
  // expect the real server sends the audio data back as a base64 encoded JSON
  // string.
  const std::vector<uint8_t> expected_output = {1, 2, 5};
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, expected_body,
      CreateServerResponse(expected_output), net::HTTP_OK);
  test_task_env_.RunUntilIdle();

  // We only get the data after the server's response. We simulate the response
  // in the code above.
  std::optional<mojom::TtsRequestError> error;
  std::vector<uint8_t> audio_data;
  std::vector<mojom::TimingInfo> timing_data;
  UnpackResult(&error, &audio_data, &timing_data,
               GetTestingObserverPtr()->GetNexResponse());
  EXPECT_EQ(audio_data, expected_output);
}

TEST_F(EnhancedNetworkTtsImplTest, GetAudioDataWithLongUtterance) {
  const std::string input_text = "Sent 1. Hello world!";
  const float rate = 1.0;
  // Sets the limit to cover the first sentence and every words in the second
  // sentence.
  GetTestingInstance().SetCharLimitPerRequestForTesting(8);
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, rate, std::nullopt, std::nullopt),
      base::BindOnce(
          [](TestAudioDataObserverImpl* observer,
             mojo::PendingReceiver<mojom::AudioDataObserver> pending_receiver) {
            observer->BindReceiver(std::move(pending_receiver));
          },
          GetTestingObserverPtr()));
  test_task_env_.RunUntilIdle();

  const std::map<std::string, std::optional<std::string>> expected_headers = {
      {kGoogApiKeyHeader, google_apis::GetReadAloudAPIKey()}};
  // |expected_output| here is arbitrary, which is encoded into a fake response
  // sent by the fake server, |TestServerURLLoaderFactory|. In general, we
  // expect the real server sends the audio data back as a base64 encoded JSON
  // string.
  const std::vector<uint8_t> expected_output = {1, 2, 5};

  // The first request contains the first sentence.
  const std::string first_expected_body =
      CreateCorrectRequest("Sent 1. ", rate);
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, first_expected_body,
      CreateServerResponse(expected_output), net::HTTP_OK);
  test_task_env_.RunUntilIdle();

  // The second request contains the first word in the second sentence.
  const std::string second_expected_body = CreateCorrectRequest("Hello", rate);
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, second_expected_body,
      CreateServerResponse(expected_output), net::HTTP_OK);
  test_task_env_.RunUntilIdle();

  // The third request contains the second word in the second sentence.
  const std::string third_expected_body = CreateCorrectRequest(" world!", rate);
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, third_expected_body,
      CreateServerResponse(expected_output), net::HTTP_OK);
  test_task_env_.RunUntilIdle();
}

TEST_F(EnhancedNetworkTtsImplTest, EmptyUtteranceError) {
  const std::string input_text("");
  const float rate = 1.0;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, rate, std::nullopt, std::nullopt),
      base::BindOnce(
          [](TestAudioDataObserverImpl* observer,
             mojo::PendingReceiver<mojom::AudioDataObserver> pending_receiver) {
            observer->BindReceiver(std::move(pending_receiver));
          },
          GetTestingObserverPtr()));
  test_task_env_.RunUntilIdle();

  // Over length request will be terminated before sending to server.
  std::optional<mojom::TtsRequestError> error;
  std::vector<uint8_t> audio_data;
  std::vector<mojom::TimingInfo> timing_data;
  UnpackResult(&error, &audio_data, &timing_data,
               GetTestingObserverPtr()->GetNexResponse());
  EXPECT_EQ(error, mojom::TtsRequestError::kEmptyUtterance);
}

TEST_F(EnhancedNetworkTtsImplTest, OverrideRequest) {
  const std::string input_text("request");
  const float rate = 1.0;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, rate, std::nullopt, std::nullopt),
      base::BindOnce(
          [](TestAudioDataObserverImpl* observer,
             mojo::PendingReceiver<mojom::AudioDataObserver> pending_receiver) {
            observer->BindReceiver(std::move(pending_receiver));
          },
          GetTestingObserverPtr()));
  test_task_env_.RunUntilIdle();
  // The second request, which has a new observer, comes in before the server
  // replies to the first one.
  TestAudioDataObserverImpl second_observer;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, rate, std::nullopt, std::nullopt),
      base::BindOnce(
          [](TestAudioDataObserverImpl* observer,
             mojo::PendingReceiver<mojom::AudioDataObserver> pending_receiver) {
            observer->BindReceiver(std::move(pending_receiver));
          },
          &second_observer));
  test_task_env_.RunUntilIdle();

  // Assume the server replies to the requests in sequence.
  const std::map<std::string, std::optional<std::string>> expected_headers = {
      {kGoogApiKeyHeader, google_apis::GetReadAloudAPIKey()}};
  std::string expected_body = CreateCorrectRequest(input_text, rate);
  const std::vector<uint8_t> expected_output = {1, 2, 5};
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, expected_body,
      CreateServerResponse(expected_output), net::HTTP_OK);
  test_task_env_.RunUntilIdle();
  // Assume the server replies same message to both requests.
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, expected_body,
      CreateServerResponse(expected_output), net::HTTP_OK);
  test_task_env_.RunUntilIdle();

  // The first request gets an error message.
  std::optional<mojom::TtsRequestError> error_first_request;
  std::vector<uint8_t> audio_data_first_request;
  std::vector<mojom::TimingInfo> timing_data_first_request;
  UnpackResult(&error_first_request, &audio_data_first_request,
               &timing_data_first_request,
               GetTestingObserverPtr()->GetNexResponse());
  EXPECT_EQ(error_first_request, mojom::TtsRequestError::kRequestOverride);
  EXPECT_EQ(timing_data_first_request.size(), 0u);
  EXPECT_EQ(audio_data_first_request.size(), 0u);

  // The second request gets the data.
  std::optional<mojom::TtsRequestError> error_second_request;
  std::vector<uint8_t> audio_data_second_request;
  std::vector<mojom::TimingInfo> timing_data_second_request;
  UnpackResult(&error_second_request, &audio_data_second_request,
               &timing_data_second_request, second_observer.GetNexResponse());
  EXPECT_EQ(audio_data_second_request, expected_output);
}

TEST_F(EnhancedNetworkTtsImplTest, ServerError) {
  const std::string input_text = "Hi.";
  const float rate = 1.0;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, rate, std::nullopt, std::nullopt),
      base::BindOnce(
          [](TestAudioDataObserverImpl* observer,
             mojo::PendingReceiver<mojom::AudioDataObserver> pending_receiver) {
            observer->BindReceiver(std::move(pending_receiver));
          },
          GetTestingObserverPtr()));
  test_task_env_.RunUntilIdle();

  const std::map<std::string, std::optional<std::string>> expected_headers = {
      {kGoogApiKeyHeader, google_apis::GetReadAloudAPIKey()}};
  const std::string expected_body = CreateCorrectRequest(input_text, rate);
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, expected_body, "" /* response= */,
      net::HTTP_INTERNAL_SERVER_ERROR);
  test_task_env_.RunUntilIdle();

  // We only get the data after the server's response. We simulate the response
  // in the code above.
  std::optional<mojom::TtsRequestError> error;
  std::vector<uint8_t> audio_data;
  std::vector<mojom::TimingInfo> timing_data;
  UnpackResult(&error, &audio_data, &timing_data,
               GetTestingObserverPtr()->GetNexResponse());
  EXPECT_EQ(error, mojom::TtsRequestError::kServerError);
}

TEST_F(EnhancedNetworkTtsImplTest, JsonDecodingError) {
  const std::string input_text = "Hi.";
  const float rate = 1.0;
  GetTestingInstance().GetAudioData(
      mojom::TtsRequest::New(input_text, rate, std::nullopt, std::nullopt),
      base::BindOnce(
          [](TestAudioDataObserverImpl* observer,
             mojo::PendingReceiver<mojom::AudioDataObserver> pending_receiver) {
            observer->BindReceiver(std::move(pending_receiver));
          },
          GetTestingObserverPtr()));
  test_task_env_.RunUntilIdle();

  const std::map<std::string, std::optional<std::string>> expected_headers = {
      {kGoogApiKeyHeader, google_apis::GetReadAloudAPIKey()}};
  const std::string expected_body = CreateCorrectRequest(input_text, rate);
  const char response[] = R"([{some wired response)";
  test_url_factory_.ExpectRequestAndSimulateResponse(
      kReadAloudServerUrl, expected_headers, expected_body, response,
      net::HTTP_OK);
  test_task_env_.RunUntilIdle();

  // We only get the data after the server's response. We simulate the response
  // in the code above.
  std::optional<mojom::TtsRequestError> error;
  std::vector<uint8_t> audio_data;
  std::vector<mojom::TimingInfo> timing_data;
  UnpackResult(&error, &audio_data, &timing_data,
               GetTestingObserverPtr()->GetNexResponse());
  EXPECT_EQ(error, mojom::TtsRequestError::kReceivedUnexpectedData);
}

}  // namespace ash::enhanced_network_tts
