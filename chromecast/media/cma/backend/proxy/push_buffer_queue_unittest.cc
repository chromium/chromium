// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/proxy/push_buffer_queue.h"

#include <atomic>
#include <optional>
#include <sstream>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cast_core/public/src/proto/runtime/cast_audio_channel_service.grpc.pb.h"

namespace chromecast {
namespace media {

class PushBufferQueueTests : public testing::Test {
 public:
  PushBufferQueueTests()
      : first_audio_buffer_(CreateAudioBufferRequest(0, false, 1, 2, 255)),
        second_audio_buffer_(CreateAudioBufferRequest(2, false, 6, 4, 2, 0)),
        third_audio_buffer_(
            CreateAudioBufferRequest(4, true, 0, 1, 1, 2, 3, 5, 8)),
        fourth_audio_buffer_(
            CreateAudioBufferRequest(42, true, 4, 8, 15, 16, 23, 42)),
        fifth_audio_buffer_(CreateAudioBufferRequest(1, false)) {
    std::vector<uint8_t> extra_data{0, 1, 7, 127, 255};

    auto* config = new cast::media::AudioConfiguration;
    config->set_codec(
        cast::media::AudioConfiguration_AudioCodec_AUDIO_CODEC_MP3);
    config->set_channel_layout(
        cast::media::
            AudioConfiguration_ChannelLayout_CHANNEL_LAYOUT_SURROUND_5_1);
    config->set_sample_format(
        cast::media::AudioConfiguration_SampleFormat_SAMPLE_FORMAT_PLANAR_S32);
    config->set_bytes_per_channel(42);
    config->set_channel_number(-1);
    config->set_samples_per_second(112358);
    config->set_extra_data(extra_data.data(), extra_data.size());

    first_audio_config_.set_allocated_audio_config(config);
  }

  void ReadData(const std::string& name,
                const PushBufferQueue::PushBufferRequest& target_buffer) {
    ASSERT_TRUE(queue_.HasBufferedData()) << name;
    std::optional<PushBufferRequest> get = queue_.GetBufferedData();
    ASSERT_TRUE(get.has_value()) << name;
    CheckEqual("first", get.value(), target_buffer);
  }

 protected:
  using PushBufferRequest = PushBufferQueue::PushBufferRequest;
  using AudioDecoderBuffer = cast::media::AudioDecoderBuffer;

  template <typename... TData>
  static PushBufferRequest CreateAudioBufferRequest(int64_t pts_micros,
                                                    bool end_of_stream,
                                                    TData... data) {
    return CreateAudioBufferRequest(
        pts_micros, end_of_stream,
        std::vector<uint8_t>{static_cast<uint8_t>(data)...});
  }

  static PushBufferRequest CreateAudioBufferRequest(
      int64_t pts_micros,
      bool end_of_stream,
      std::vector<uint8_t> data_vector) {
    PushBufferRequest request;
    auto* audio_buffer = new AudioDecoderBuffer;
    audio_buffer->set_pts_micros(pts_micros);
    audio_buffer->set_end_of_stream(end_of_stream);
    audio_buffer->set_data(data_vector.data(), data_vector.size());

    request.set_allocated_buffer(audio_buffer);
    return request;
  }

  void CheckEqual(const std::string& name,
                  const PushBufferRequest& first,
                  const PushBufferRequest& second) {
    std::string failure_str = "failed on " + std::move(name);

    ASSERT_EQ(first.has_buffer(), second.has_buffer()) << failure_str;
    ASSERT_EQ(first.has_audio_config(), second.has_audio_config())
        << failure_str;

    if (first.has_buffer()) {
      EXPECT_EQ(first.buffer().pts_micros(), second.buffer().pts_micros())
          << failure_str;
      EXPECT_EQ(first.buffer().end_of_stream(), second.buffer().end_of_stream())
          << failure_str;
      EXPECT_EQ(first.buffer().data(), second.buffer().data()) << failure_str;
    }

    if (first.has_audio_config()) {
      EXPECT_EQ(first.audio_config().codec(), second.audio_config().codec())
          << failure_str;
      EXPECT_EQ(first.audio_config().channel_layout(),
                second.audio_config().channel_layout())
          << failure_str;
      EXPECT_EQ(first.audio_config().sample_format(),
                second.audio_config().sample_format())
          << failure_str;
      EXPECT_EQ(first.audio_config().bytes_per_channel(),
                second.audio_config().bytes_per_channel())
          << failure_str;
      EXPECT_EQ(first.audio_config().channel_number(),
                second.audio_config().channel_number())
          << failure_str;
      EXPECT_EQ(first.audio_config().samples_per_second(),
                second.audio_config().samples_per_second())
          << failure_str;
      EXPECT_EQ(first.audio_config().extra_data(),
                second.audio_config().extra_data())
          << failure_str;
    }
  }

  void UpdateBufferWriteStreamPositions() {
    queue_.producer_handler_.overflow();
    queue_.producer_handler_.ApplyNewBytesWritten();
  }

  std::string GetIterationName(int iteration_id) {
    std::stringstream ss;
    ss << "iteration " << iteration_id;
    return ss.str();
  }

  bool StartPushBuffer(const PushBufferRequest& request) {
    return queue_.PushBufferImpl(request);
  }

  std::optional<PushBufferQueue::PushBufferRequest> StartGetBufferedData() {
    return queue_.GetBufferedDataImpl();
  }

  void FinishPushBuffer() { queue_.producer_handler_.ApplyNewBytesWritten(); }

  void FinishGetBufferedData() { queue_.consumer_handler_.ApplyNewBytesRead(); }

  std::istream* consumer_stream() { return &queue_.consumer_stream_.value(); }
  std::ostream* producer_stream() { return &queue_.producer_stream_.value(); }

  size_t available_bytes() { return queue_.GetAvailableyByteCount(); }

  PushBufferQueue queue_;

  // Some test data
  PushBufferRequest first_audio_buffer_;
  PushBufferRequest second_audio_buffer_;
  PushBufferRequest third_audio_buffer_;
  PushBufferRequest fourth_audio_buffer_;
  PushBufferRequest fifth_audio_buffer_;
  PushBufferRequest first_audio_config_;
};

TEST_F(PushBufferQueueTests, TestPushOrdering) {
  EXPECT_FALSE(queue_.HasBufferedData());

  queue_.PushBuffer(first_audio_buffer_);
  queue_.PushBuffer(second_audio_buffer_);
  queue_.PushBuffer(first_audio_config_);
  queue_.PushBuffer(third_audio_buffer_);

  ReadData("first", first_audio_buffer_);
  ReadData("second", second_audio_buffer_);

  queue_.PushBuffer(fourth_audio_buffer_);

  ReadData("config", first_audio_config_);
  ReadData("third", third_audio_buffer_);
  ReadData("fourth", fourth_audio_buffer_);

  EXPECT_FALSE(queue_.HasBufferedData());
  queue_.PushBuffer(fifth_audio_buffer_);

  ReadData("fifth", fifth_audio_buffer_);

  EXPECT_FALSE(queue_.HasBufferedData());
}

TEST_F(PushBufferQueueTests, TestPushLargeBuffer) {
  std::vector<uint8_t> data;
  for (int i = 0; i < 256; i++) {
    data.push_back(i);
  }

  auto buffer = CreateAudioBufferRequest(0, false, data);

  queue_.PushBuffer(buffer);

  ReadData("big buffer", buffer);
}

TEST_F(PushBufferQueueTests, TestWrapAround) {
  auto first_buffer = CreateAudioBufferRequest(0, false, 0, 1);
  queue_.PushBuffer(first_buffer);

  for (size_t i = 1; i < PushBufferQueue::kBufferSizeBytes * 3; i++) {
    const std::string name = GetIterationName(i);
    const uint8_t previous_id = (i - 1) % 256;
    const uint8_t current_id = i % 256;
    const uint8_t next_id = (i + 1) % 256;
    auto buffer = CreateAudioBufferRequest(
        0, false, std::vector<uint8_t>{current_id, next_id});
    auto old_buffer = CreateAudioBufferRequest(
        0, false, std::vector<uint8_t>{previous_id, current_id});

    // Make sure the length is 6. 7 is prime, so guaranteed to hit all possible
    // positions in |buffer_| when an extra bit is used for the size.
    std::string serialized_str;
    buffer.SerializeToString(&serialized_str);
    ASSERT_EQ(serialized_str.size(), size_t{6}) << name;

    ASSERT_TRUE(queue_.HasBufferedData()) << name;
    ASSERT_EQ(queue_.PushBuffer(buffer),
              CmaBackend::BufferStatus::kBufferSuccess)
        << name;

    ReadData(name, old_buffer);
  }
}

TEST_F(PushBufferQueueTests, TestWriteEntireBuffer) {
  for (size_t i = 0; i < (PushBufferQueue::kBufferSizeBytes >> 3); i++) {
    auto buffer = CreateAudioBufferRequest(0, false, 0, 1, 2);

    // Make sure the length is 8 after serialization (with the extra length
    // bit).
    std::string serialized_str;
    buffer.SerializeToString(&serialized_str);
    ASSERT_EQ(serialized_str.size(), size_t{7});
    ASSERT_EQ(queue_.PushBuffer(buffer),
              CmaBackend::BufferStatus::kBufferSuccess)
        << GetIterationName(i);
  }

  auto failing_buffer = CreateAudioBufferRequest(0, false);
  EXPECT_EQ(queue_.PushBuffer(failing_buffer),
            CmaBackend::BufferStatus::kBufferFailed);

  for (size_t i = 0; i < (PushBufferQueue::kBufferSizeBytes >> 3); i++) {
    auto buffer = CreateAudioBufferRequest(0, false, 0, 1, 2);
    ReadData(GetIterationName(i), buffer);
  }

  // Make sure writing still works after the failed write above.
  EXPECT_FALSE(queue_.HasBufferedData());
  queue_.PushBuffer(first_audio_buffer_);

  ReadData("first", first_audio_buffer_);

  EXPECT_FALSE(queue_.HasBufferedData());
}

TEST_F(PushBufferQueueTests, TestReadingFromPartialWrite) {
  std::string serialized_str;
  first_audio_buffer_.SerializeToString(&serialized_str);
  char size = static_cast<char>(serialized_str.size());
  ASSERT_GT(size, 2);

  *producer_stream() << size << serialized_str[0] << serialized_str[1];
  UpdateBufferWriteStreamPositions();

  ASSERT_TRUE(queue_.HasBufferedData());
  std::optional<PushBufferRequest> pulled_buffer = queue_.GetBufferedData();
  EXPECT_FALSE(pulled_buffer.has_value());
  EXPECT_TRUE(queue_.HasBufferedData());

  for (size_t i = 2; i < serialized_str.size(); i++) {
    *producer_stream() << serialized_str[i];
  }
  UpdateBufferWriteStreamPositions();

  ASSERT_TRUE(queue_.HasBufferedData());
  pulled_buffer = queue_.GetBufferedData();
  ASSERT_TRUE(pulled_buffer.has_value());
  CheckEqual("buffer", pulled_buffer.value(), first_audio_buffer_);
}

TEST_F(PushBufferQueueTests, InterleaveProduceAndConsume) {
  EXPECT_FALSE(queue_.HasBufferedData());

  EXPECT_TRUE(StartPushBuffer(first_audio_buffer_));
  EXPECT_FALSE(queue_.HasBufferedData());

  FinishPushBuffer();

  ReadData("first", first_audio_buffer_);

  ASSERT_TRUE(StartPushBuffer(second_audio_buffer_));

  FinishGetBufferedData();

  EXPECT_FALSE(queue_.HasBufferedData());

  FinishPushBuffer();

  ReadData("second", second_audio_buffer_);

  EXPECT_FALSE(queue_.HasBufferedData());
}

TEST_F(PushBufferQueueTests, TestMultithreaded) {
  queue_.PushBuffer(first_audio_buffer_);
  queue_.PushBuffer(second_audio_buffer_);

  base::Thread consumer_thread("Consumer Thread");
  consumer_thread.StartAndWaitForTesting();
  {
    auto task_runner = consumer_thread.task_runner();
    auto this_ptr = base::Unretained(this);
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&PushBufferQueueTests::ReadData, this_ptr,
                                  "first", first_audio_buffer_));

    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&PushBufferQueueTests::ReadData, this_ptr,
                                  "second", second_audio_buffer_));

    queue_.PushBuffer(third_audio_buffer_);
    queue_.PushBuffer(fourth_audio_buffer_);
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&PushBufferQueueTests::ReadData, this_ptr,
                                  "third", third_audio_buffer_));
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&PushBufferQueueTests::ReadData, this_ptr,
                                  "fourth", fourth_audio_buffer_));
  }

  consumer_thread.FlushForTesting();
  consumer_thread.Stop();
}

}  // namespace media
}  // namespace chromecast
