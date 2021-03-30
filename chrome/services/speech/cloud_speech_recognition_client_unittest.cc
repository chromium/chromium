// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/speech/cloud_speech_recognition_client.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/services/speech/speech_recognition_service_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "content/public/browser/google_streaming_api.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "media/base/bind_to_current_loop.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_error.mojom.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_result.mojom.h"

using base::checked_cast;
using base::HostToNet32;

namespace speech {

// The number of bytes in the dummy audio.
constexpr uint32_t kDummyAudioBytes = 4000;

class SpeechRecognitionServiceImplMock : public SpeechRecognitionServiceImpl {
 public:
  explicit SpeechRecognitionServiceImplMock(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionService> receiver);

  mojo::PendingRemote<network::mojom::URLLoaderFactory> GetUrlLoaderFactory()
      override;
  std::vector<network::TestURLLoaderFactory::PendingRequest>*
  GetPendingRequests();
  int GetNumPending();

  void ResetNetwork();

  // private:
  // Instantiate a TestURLLoaderFactory which we can use to respond and unpause
  // network requests.
  network::TestURLLoaderFactory test_url_loader_factory_;
  mojo::Receiver<network::mojom::URLLoaderFactory> test_factory_receiver_;
};

class CloudSpeechRecognitionClientUnitTest : public testing::Test {
 public:
  CloudSpeechRecognitionClientUnitTest();

  // testing::Test methods.
  void SetUp() override;

 protected:
  void OnRecognitionEvent(const std::string& result, const bool is_final);

  void InjectDummyAudio();

  void InitializeUpstreamPipeIfNecessary();

  // Reads and returns all pending upload data from |upstream_data_pipe_|,
  // initializing the pipe from |GetUpstreamRequest()|, if needed.
  std::string ConsumeChunkedUploadData(uint32_t expected_num_bytes);

  const network::TestURLLoaderFactory::PendingRequest* GetUpstreamRequest();
  const network::TestURLLoaderFactory::PendingRequest* GetDownstreamRequest();

  void ProvideMockResponseStartDownstreamIfNeeded();
  void ProvideMockStringResponseDownstream(const std::string& response_string);
  void ProvideMockProtoResultDownstream(
      const content::proto::SpeechRecognitionEvent& result);
  void ProvideMockResultDownstream(std::vector<std::string> result_strings,
                                   bool is_final);
  static std::string SerializeProtobufResponse(
      const content::proto::SpeechRecognitionEvent& msg);
  void ExpectResultsReceived(const std::vector<std::string>& expected_results,
                             bool is_final);

  std::unique_ptr<CloudSpeechRecognitionClient> client_under_test_;
  std::unique_ptr<SpeechRecognitionServiceImplMock>
      speech_recognition_service_impl_;
  mojo::Remote<media::mojom::SpeechRecognitionService> remote_;

  mojo::ScopedDataPipeProducerHandle downstream_data_pipe_;
  mojo::Remote<network::mojom::ChunkedDataPipeGetter> chunked_data_pipe_getter_;
  mojo::ScopedDataPipeConsumerHandle upstream_data_pipe_;
  base::queue<std::string> results_;
  bool is_final_ = false;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

SpeechRecognitionServiceImplMock::SpeechRecognitionServiceImplMock(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionService> receiver)
    : SpeechRecognitionServiceImpl(std::move(receiver)),
      test_factory_receiver_(&test_url_loader_factory_) {
  TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_));
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
SpeechRecognitionServiceImplMock::GetUrlLoaderFactory() {
  return test_factory_receiver_.BindNewPipeAndPassRemote();
}

std::vector<network::TestURLLoaderFactory::PendingRequest>*
SpeechRecognitionServiceImplMock::GetPendingRequests() {
  return test_url_loader_factory_.pending_requests();
}

int SpeechRecognitionServiceImplMock::GetNumPending() {
  return test_url_loader_factory_.NumPending();
}

void SpeechRecognitionServiceImplMock::ResetNetwork() {
  test_factory_receiver_.reset();
}

CloudSpeechRecognitionClientUnitTest::CloudSpeechRecognitionClientUnitTest() =
    default;

void CloudSpeechRecognitionClientUnitTest::SetUp() {
  client_under_test_ = std::make_unique<CloudSpeechRecognitionClient>(
      media::BindToCurrentLoop(base::BindRepeating(
          &CloudSpeechRecognitionClientUnitTest::OnRecognitionEvent,
          base::Unretained(this))),
      nullptr);

  speech_recognition_service_impl_ =
      std::make_unique<SpeechRecognitionServiceImplMock>(
          remote_.BindNewPipeAndPassReceiver());

  client_under_test_->SetUrlLoaderFactoryForTesting(
      speech_recognition_service_impl_->GetUrlLoaderFactory());

  CloudSpeechConfig config;
  config.sample_rate = 48000;
  config.channel_count = 2;
  config.language_code = "en-US";
  client_under_test_->Initialize(config);

  // Run the loop to guarantee that the upstream and downstream loaders have
  // started.
  while (!GetUpstreamRequest() || !GetDownstreamRequest()) {
    task_environment_.RunUntilIdle();
  }
}

void CloudSpeechRecognitionClientUnitTest::OnRecognitionEvent(
    const std::string& result,
    const bool is_final) {
  results_.push(result);
  is_final_ = is_final;
}

void CloudSpeechRecognitionClientUnitTest::InjectDummyAudio() {
  DCHECK(client_under_test_.get());
  char dummy_audio_buffer_data[kDummyAudioBytes] = {'\0'};
  client_under_test_->AddAudio(base::span<char>(
      &dummy_audio_buffer_data[0], sizeof(dummy_audio_buffer_data)));
}

void CloudSpeechRecognitionClientUnitTest::InitializeUpstreamPipeIfNecessary() {
  if (!upstream_data_pipe_.get()) {
    if (!chunked_data_pipe_getter_) {
      const network::TestURLLoaderFactory::PendingRequest* upstream_request =
          GetUpstreamRequest();
      EXPECT_TRUE(upstream_request);
      EXPECT_TRUE(upstream_request->request.request_body);
      auto& mutable_elements =
          *upstream_request->request.request_body->elements_mutable();
      ASSERT_EQ(1u, mutable_elements.size());
      ASSERT_EQ(network::DataElement::Tag::kChunkedDataPipe,
                mutable_elements[0].type());
      chunked_data_pipe_getter_.Bind(
          mutable_elements[0]
              .As<network::DataElementChunkedDataPipe>()
              .ReleaseChunkedDataPipeGetter());
    }

    constexpr size_t kDataPipeCapacity = 256;
    const MojoCreateDataPipeOptions data_pipe_options{
        sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1,
        kDataPipeCapacity};
    mojo::ScopedDataPipeProducerHandle producer_end;
    mojo::ScopedDataPipeConsumerHandle consumer_end;
    CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&data_pipe_options,
                                                  producer_end, consumer_end));
    chunked_data_pipe_getter_->StartReading(std::move(producer_end));
    upstream_data_pipe_ = std::move(consumer_end);
  }
}

std::string CloudSpeechRecognitionClientUnitTest::ConsumeChunkedUploadData(
    uint32_t expected_num_bytes) {
  std::string result;
  InitializeUpstreamPipeIfNecessary();

  EXPECT_TRUE(upstream_data_pipe_.is_valid());

  std::string out;
  while (true) {
    task_environment_.RunUntilIdle();

    const void* data;
    uint32_t num_bytes = 0;
    MojoResult result = upstream_data_pipe_->BeginReadData(
        &data, &num_bytes, MOJO_READ_DATA_FLAG_NONE);

    expected_num_bytes -= num_bytes;
    if (result == MOJO_RESULT_OK) {
      out.append(static_cast<const char*>(data), num_bytes);
      upstream_data_pipe_->EndReadData(num_bytes);
      continue;
    }

    if (result == MOJO_RESULT_SHOULD_WAIT) {
      if (expected_num_bytes > 0) {
        continue;
      } else {
        break;
      }
    }

    LOG(INFO) << "Mojo pipe unexpectedly closed with result:" << result;
    break;
  }

  return out;
}

// Returns the latest upstream request.
const network::TestURLLoaderFactory::PendingRequest*
CloudSpeechRecognitionClientUnitTest::GetUpstreamRequest() {
  auto* pending_requests =
      speech_recognition_service_impl_->GetPendingRequests();
  for (int i = pending_requests->size() - 1; i >= 0; i--) {
    const auto& pending_request = (*pending_requests)[i];
    if (pending_request.request.url.spec().find("/up") != std::string::npos)
      return &pending_request;
  }

  return nullptr;
}

// Returns the latest downstream request.
const network::TestURLLoaderFactory::PendingRequest*
CloudSpeechRecognitionClientUnitTest::GetDownstreamRequest() {
  auto* pending_requests =
      speech_recognition_service_impl_->GetPendingRequests();
  for (int i = pending_requests->size() - 1; i >= 0; i--) {
    const auto& pending_request = (*pending_requests)[i];
    if (pending_request.request.url.spec().find("/down") != std::string::npos)
      return &pending_request;
  }

  return nullptr;
}

void CloudSpeechRecognitionClientUnitTest::
    ProvideMockResponseStartDownstreamIfNeeded() {
  if (downstream_data_pipe_.get())
    return;
  const network::TestURLLoaderFactory::PendingRequest* downstream_request =
      GetDownstreamRequest();
  ASSERT_TRUE(downstream_request);

  auto head = network::mojom::URLResponseHead::New();
  std::string headers("HTTP/1.1 200 OK\n\n");
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  downstream_request->client->OnReceiveResponse(std::move(head));

  constexpr size_t kDataPipeCapacity = 256;
  const MojoCreateDataPipeOptions data_pipe_options{
      sizeof(MojoCreateDataPipeOptions), MOJO_CREATE_DATA_PIPE_FLAG_NONE, 1,
      kDataPipeCapacity};
  mojo::ScopedDataPipeProducerHandle producer_end;
  mojo::ScopedDataPipeConsumerHandle consumer_end;
  CHECK_EQ(MOJO_RESULT_OK, mojo::CreateDataPipe(&data_pipe_options,
                                                producer_end, consumer_end));
  downstream_request->client->OnStartLoadingResponseBody(
      std::move(consumer_end));
  downstream_data_pipe_ = std::move(producer_end);
}

void CloudSpeechRecognitionClientUnitTest::ProvideMockStringResponseDownstream(
    const std::string& response_string) {
  ProvideMockResponseStartDownstreamIfNeeded();
  uint32_t written = 0;
  while (written < response_string.size()) {
    uint32_t write_bytes = response_string.size() - written;
    MojoResult result = downstream_data_pipe_->WriteData(
        response_string.data() + written, &write_bytes,
        MOJO_WRITE_DATA_FLAG_NONE);
    if (result == MOJO_RESULT_OK) {
      written += write_bytes;
      continue;
    }

    if (result == MOJO_RESULT_SHOULD_WAIT) {
      task_environment_.RunUntilIdle();
      continue;
    }

    FAIL() << "Mojo write failed unexpectedly with result:" << result;
    return;
  }

  // Flush the mojo pipe.
  task_environment_.RunUntilIdle();
}

void CloudSpeechRecognitionClientUnitTest::ProvideMockProtoResultDownstream(
    const content::proto::SpeechRecognitionEvent& result) {
  ProvideMockResponseStartDownstreamIfNeeded();
  ASSERT_TRUE(downstream_data_pipe_.get());
  ASSERT_TRUE(downstream_data_pipe_.is_valid());

  ProvideMockStringResponseDownstream(SerializeProtobufResponse(result));
}

void CloudSpeechRecognitionClientUnitTest::ProvideMockResultDownstream(
    std::vector<std::string> result_strings,
    bool is_final) {
  std::vector<blink::mojom::SpeechRecognitionResultPtr> results;
  results.push_back(blink::mojom::SpeechRecognitionResult::New());
  blink::mojom::SpeechRecognitionResultPtr& result = results.back();
  result->is_provisional = false;

  for (std::string result_string : result_strings) {
    result->hypotheses.push_back(blink::mojom::SpeechRecognitionHypothesis::New(
        base::UTF8ToUTF16(result_string), 0.1F));
  }

  content::proto::SpeechRecognitionEvent proto_event;
  proto_event.set_status(
      content::proto::SpeechRecognitionEvent::STATUS_SUCCESS);
  content::proto::SpeechRecognitionResult* proto_result =
      proto_event.add_result();
  proto_result->set_final(is_final);
  proto_result->set_stability(1.0);
  for (const auto& hypothesis : result->hypotheses) {
    content::proto::SpeechRecognitionAlternative* proto_alternative =
        proto_result->add_alternative();
    proto_alternative->set_confidence(hypothesis->confidence);
    proto_alternative->set_transcript(base::UTF16ToUTF8(hypothesis->utterance));
  }

  ProvideMockProtoResultDownstream(proto_event);
}

std::string CloudSpeechRecognitionClientUnitTest::SerializeProtobufResponse(
    const content::proto::SpeechRecognitionEvent& msg) {
  std::string msg_string;
  msg.SerializeToString(&msg_string);

  // Prepend 4 byte prefix length indication to the protobuf message as
  // envisaged by the google streaming recognition webservice protocol.
  uint32_t prefix = HostToNet32(checked_cast<uint32_t>(msg_string.size()));
  msg_string.insert(0, reinterpret_cast<char*>(&prefix), sizeof(prefix));

  return msg_string;
}

void CloudSpeechRecognitionClientUnitTest::ExpectResultsReceived(
    const std::vector<std::string>& expected_results,
    bool is_final) {
  ASSERT_GE(1U, results_.size());
  std::string expected_transcription;
  for (std::string result : expected_results) {
    expected_transcription += result;
  }

  ASSERT_EQ(is_final, is_final_);
  ASSERT_TRUE(!expected_transcription.empty());
  ASSERT_TRUE(expected_transcription == results_.front());
  results_.pop();
}

TEST_F(CloudSpeechRecognitionClientUnitTest, StreamingRecognition) {
  ASSERT_TRUE(client_under_test_->IsInitialized());
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_TRUE(GetDownstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData(0));

  InjectDummyAudio();
  ASSERT_FALSE(ConsumeChunkedUploadData(kDummyAudioBytes).empty());

  // Simulate a protobuf message streamed from the server containing a single
  // result with two hypotheses.
  std::vector<std::string> result_strings;
  result_strings.push_back("hypothesis 1");
  result_strings.push_back("hypothesis 2");

  bool is_final = false;
  ProvideMockResultDownstream(result_strings, is_final);
  ExpectResultsReceived(result_strings, is_final);
}

TEST_F(CloudSpeechRecognitionClientUnitTest, DidAudioPropertyChange) {
  ASSERT_FALSE(client_under_test_->DidAudioPropertyChange(48000, 2));
  ASSERT_TRUE(client_under_test_->DidAudioPropertyChange(48000, 1));
  ASSERT_TRUE(client_under_test_->DidAudioPropertyChange(44100, 2));
  ASSERT_TRUE(client_under_test_->DidAudioPropertyChange(44100, 1));
}

// Verifies that invalid response strings are handled appropriately.
TEST_F(CloudSpeechRecognitionClientUnitTest, InvalidResponseString) {
  ASSERT_TRUE(client_under_test_->IsInitialized());
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_TRUE(GetDownstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData(0));

  InjectDummyAudio();
  ASSERT_FALSE(ConsumeChunkedUploadData(kDummyAudioBytes).empty());

  ProvideMockStringResponseDownstream("INVALID RESPONSE STRING");
  ASSERT_TRUE(results_.empty());
}

// Verifies that the client gracefully recovers from network crashes.
TEST_F(CloudSpeechRecognitionClientUnitTest, NetworkReset) {
  ASSERT_TRUE(client_under_test_->IsInitialized());
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_TRUE(GetDownstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData(0));

  InjectDummyAudio();
  ASSERT_FALSE(ConsumeChunkedUploadData(kDummyAudioBytes).empty());

  // Simulate a network crash by resetting the URL loader factory receiver.
  speech_recognition_service_impl_->ResetNetwork();
  InjectDummyAudio();
  ASSERT_FALSE(ConsumeChunkedUploadData(kDummyAudioBytes).empty());
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_TRUE(GetDownstreamRequest());

  // Simulate a protobuf message streamed from the server containing a single
  // result with one hypothesis.
  std::vector<std::string> result_strings;
  result_strings.push_back("hypothesis 1");

  bool is_final = false;
  ProvideMockResultDownstream(result_strings, is_final);
  ExpectResultsReceived(result_strings, is_final);
}

// Verifies that the stream is reset after 295 seconds. The Open Speech API
// supports a maximum recognition time of 5 minutes, so we must reset the stream
// with a new request key before then.
TEST_F(CloudSpeechRecognitionClientUnitTest, StreamReset) {
  ASSERT_TRUE(client_under_test_->IsInitialized());
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_TRUE(GetDownstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData(0));

  std::string UploadUrlBeforeReset = GetUpstreamRequest()->request.url.spec();
  std::string DownloadUrlBeforeReset =
      GetDownstreamRequest()->request.url.spec();

  // Fast forward by 325 total seconds to trigger a reset.
  for (int i = 0; i < 13; i++) {
    InjectDummyAudio();
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(25));
  }

  ASSERT_EQ(2, speech_recognition_service_impl_->GetNumPending());

  std::string UploadUrlAfterReset = GetUpstreamRequest()->request.url.spec();
  std::string DownloadUrlAfterReset =
      GetDownstreamRequest()->request.url.spec();

  // The URLs after the reset should contain a different request key.
  ASSERT_NE(UploadUrlBeforeReset, UploadUrlAfterReset);
  ASSERT_NE(DownloadUrlBeforeReset, DownloadUrlAfterReset);
}

// Verifies that the stream is reset if the audio is paused for longer than 30
// seconds.
TEST_F(CloudSpeechRecognitionClientUnitTest, StreamResetAfterPause) {
  ASSERT_TRUE(client_under_test_->IsInitialized());
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_TRUE(GetDownstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData(0));

  InjectDummyAudio();
  std::string UploadUrlBeforeReset = GetUpstreamRequest()->request.url.spec();
  std::string DownloadUrlBeforeReset =
      GetDownstreamRequest()->request.url.spec();

  // Fast forward by 35 seconds to trigger a reset.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(35));
  InjectDummyAudio();
  ASSERT_EQ(2, speech_recognition_service_impl_->GetNumPending());

  std::string UploadUrlAfterReset = GetUpstreamRequest()->request.url.spec();
  std::string DownloadUrlAfterReset =
      GetDownstreamRequest()->request.url.spec();

  // The URLs after the reset should contain a different request key.
  ASSERT_NE(UploadUrlBeforeReset, UploadUrlAfterReset);
  ASSERT_NE(DownloadUrlBeforeReset, DownloadUrlAfterReset);
}

TEST_F(CloudSpeechRecognitionClientUnitTest, FinalRecognitionResult) {
  ASSERT_TRUE(client_under_test_->IsInitialized());
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_TRUE(GetDownstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData(0));

  InjectDummyAudio();
  ASSERT_FALSE(ConsumeChunkedUploadData(kDummyAudioBytes).empty());

  // Simulate a protobuf message streamed from the server containing a single
  // result.
  std::vector<std::string> result_strings;
  result_strings.push_back("hypothesis 2");

  bool is_final = true;
  ProvideMockResultDownstream(result_strings, is_final);
  ExpectResultsReceived(result_strings, is_final);
}

// Verify that the leading whitespace is trimmed.
TEST_F(CloudSpeechRecognitionClientUnitTest, TrimLeadingWhitespace) {
  ASSERT_TRUE(client_under_test_->IsInitialized());
  ASSERT_TRUE(GetUpstreamRequest());
  ASSERT_TRUE(GetDownstreamRequest());
  ASSERT_EQ("", ConsumeChunkedUploadData(0));

  InjectDummyAudio();
  ASSERT_FALSE(ConsumeChunkedUploadData(kDummyAudioBytes).empty());

  // Simulate a protobuf message streamed from the server containing two
  // results.
  std::vector<std::string> result_strings;
  result_strings.push_back(" hypothesis 1");
  result_strings.push_back(" hypothesis 2");

  std::vector<std::string> expected_result_strings;
  expected_result_strings.push_back("hypothesis 1");
  expected_result_strings.push_back(" hypothesis 2");

  bool is_final = false;
  ProvideMockResultDownstream(result_strings, is_final);
  ExpectResultsReceived(expected_result_strings, is_final);
}

}  // namespace speech
