// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/services/speech/soda/soda_client_impl.h"

#include <algorithm>
#include <memory>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/services/speech/soda/proto/soda_api.pb.h"
#include "chrome/services/speech/soda/soda_test_paths.h"
#include "media/audio/wav_audio_handler.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace soda {

class SodaClientImplUnitTest : public testing::Test {
 public:
  SodaClientImplUnitTest() = default;
  ~SodaClientImplUnitTest() override = default;

  void AddRecognitionResult(std::string result);

  std::string WaitForRecognitionResult() { return result_future_.Take(); }

 protected:
  void SetUp() override;
  void TearDown() override;

  static void OnSodaResponse(const char* serialized_proto,
                             int length,
                             void* callback_handle);
  void OnRecognitionResultOnMainThread(std::string result);
  void OnStopReceived();
  void OnStopReceivedOnMainThread();

  base::test::SingleThreadTaskEnvironment task_environment_;
  // The root directory for test files.
  base::FilePath test_data_dir_;
  std::unique_ptr<soda::SodaClientImpl> soda_client_;
  std::vector<std::string> recognition_results_;
  base::test::TestFuture<std::string> result_future_;
  base::WeakPtr<SodaClientImplUnitTest> weak_this_;
  base::WeakPtrFactory<SodaClientImplUnitTest> weak_factory_{this};
};

// static
void SodaClientImplUnitTest::OnSodaResponse(const char* serialized_proto,
                                            int length,
                                            void* callback_handle) {
  if (!callback_handle) {
    return;
  }
  speech::soda::chrome::SodaResponse response;
  if (!response.ParseFromArray(serialized_proto, length)) {
    LOG(ERROR) << "Unable to parse result from SODA.";
    return;
  }

  if (response.soda_type() == speech::soda::chrome::SodaResponse::RECOGNITION) {
    speech::soda::chrome::SodaRecognitionResult result =
        response.recognition_result();
    if (result.hypothesis_size() > 0) {
      static_cast<soda::SodaClientImplUnitTest*>(callback_handle)
          ->AddRecognitionResult(result.hypothesis(0));
    }
  }

  if (response.soda_type() == speech::soda::chrome::SodaResponse::STOP) {
    static_cast<soda::SodaClientImplUnitTest*>(callback_handle)
        ->OnStopReceived();
  }
}

void SodaClientImplUnitTest::AddRecognitionResult(std::string result) {
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SodaClientImplUnitTest::OnRecognitionResultOnMainThread,
                     weak_this_, std::move(result)));
}

void SodaClientImplUnitTest::OnRecognitionResultOnMainThread(
    std::string result) {
  std::erase_if(result, [](char c) { return base::IsAsciiPunctuation(c); });
  recognition_results_.push_back(std::move(result));
  if (recognition_results_.back().find("Hey Google Hey Google") !=
      std::string::npos) {
    if (!result_future_.IsReady()) {
      result_future_.SetValue(recognition_results_.back());
    }
  }
}

void SodaClientImplUnitTest::OnStopReceived() {
  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&SodaClientImplUnitTest::OnStopReceivedOnMainThread,
                     weak_this_));
}

void SodaClientImplUnitTest::OnStopReceivedOnMainThread() {
  if (!result_future_.IsReady()) {
    result_future_.SetValue(recognition_results_.empty()
                                ? std::string()
                                : recognition_results_.back());
  }
}

void SodaClientImplUnitTest::SetUp() {
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir_));
  weak_this_ = weak_factory_.GetWeakPtr();
#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
  // TODO(crbug.com/40753481): Enable test once arm64 macOS binary is available
  // in CIPD.
  GTEST_SKIP()
      << "SODA test binary for arm64 macOS is currently being rolled in CIPD.";
#else
  auto libsoda_path =
      test_data_dir_.Append(base::FilePath(soda::kSodaResourcePath))
          .Append(base::FilePath(soda::kSodaTestBinaryRelativePath));
  ASSERT_TRUE(base::PathExists(libsoda_path));
  soda_client_ = std::make_unique<soda::SodaClientImpl>(libsoda_path);
  ASSERT_TRUE(soda_client_.get());
  ASSERT_FALSE(soda_client_->IsInitialized());
#endif
}

void SodaClientImplUnitTest::TearDown() {
  soda_client_.reset();
  testing::Test::TearDown();
}

TEST_F(SodaClientImplUnitTest, CreateSodaClient) {
  auto audio_file =
      test_data_dir_.Append(base::FilePath(soda::kSodaResourcePath))
          .Append(base::FilePath(soda::kSodaTestAudioRelativePath));
  ASSERT_TRUE(base::PathExists(audio_file));

  std::string buffer;
  ASSERT_TRUE(base::ReadFileToString(audio_file, &buffer));

  auto handler = media::WavAudioHandler::Create(base::as_byte_span(buffer));
  ASSERT_TRUE(handler.get());
  ASSERT_EQ(handler->GetNumChannels(), 1);

  auto config_file_path =
      test_data_dir_.Append(base::FilePath(soda::kSodaResourcePath))
          .Append(base::FilePath(soda::kSodaLanguagePackRelativePath));
  ASSERT_TRUE(base::PathExists(config_file_path));

  speech::soda::chrome::ExtendedSodaConfigMsg config_msg;
  config_msg.set_channel_count(handler->GetNumChannels());
  config_msg.set_sample_rate(handler->GetSampleRate());
  config_msg.set_language_pack_directory(
      config_file_path.AsUTF8Unsafe().c_str());
  config_msg.set_simulate_realtime_testonly(false);
  config_msg.set_enable_lang_id(false);
  config_msg.set_recognition_mode(
      speech::soda::chrome::ExtendedSodaConfigMsg::CAPTION);

  // The test binary does not verify the execution context.
  config_msg.set_api_key("");

  auto serialized = config_msg.SerializeAsString();

  SerializedSodaConfig config;
  config.soda_config = serialized.c_str();
  config.soda_config_size = serialized.size();
  config.callback = &OnSodaResponse;
  config.callback_handle = this;
  soda_client_->Reset(config, handler->GetSampleRate(),
                      handler->GetNumChannels());
  ASSERT_TRUE(soda_client_->IsInitialized());

  auto bus = media::AudioBus::Create(handler->GetNumChannels(),
                                     handler->total_frames_for_testing());

  size_t bytes_written = 0u;
  ASSERT_TRUE(handler->CopyTo(bus.get(), &bytes_written));

  std::vector<int16_t> audio_data(bus->frames());
  bus->ToInterleaved<media::SignedInt16SampleTypeTraits>(audio_data);

  constexpr size_t kMaxChunkSize = 1024;
  constexpr size_t kReplayAudioCount = 2;

  for (size_t i = 0; i < kReplayAudioCount; i++) {
    size_t chunk_start = 0;
    // Upload chunks of 1024 frames at a time.
    while (chunk_start < audio_data.size()) {
      size_t chunk_size =
          std::min(kMaxChunkSize, audio_data.size() - chunk_start);
      soda_client_->AddAudio(
          reinterpret_cast<const char*>(&audio_data[chunk_start]),
          sizeof(int16_t) * chunk_size);

      chunk_start += chunk_size;
    }
  }

  soda_client_->MarkDone();

  EXPECT_EQ(WaitForRecognitionResult(), "Hey Google Hey Google");
  ASSERT_GT(recognition_results_.size(), kReplayAudioCount);
  ASSERT_EQ(recognition_results_.back(), "Hey Google Hey Google");
}

}  // namespace soda
