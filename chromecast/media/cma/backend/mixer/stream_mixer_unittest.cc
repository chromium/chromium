// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/stream_mixer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromecast/media/audio/mixer_service/loopback_connection.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/media/audio/mixer_service/redirected_audio_connection.h"
#include "chromecast/media/cma/backend/mixer/audio_output_redirector.h"
#include "chromecast/media/cma/backend/mixer/channel_layout.h"
#include "chromecast/media/cma/backend/mixer/loopback_handler.h"
#include "chromecast/media/cma/backend/mixer/loopback_test_support.h"
#include "chromecast/media/cma/backend/mixer/mixer_input.h"
#include "chromecast/media/cma/backend/mixer/mock_mixer_source.h"
#include "chromecast/media/cma/backend/mixer/mock_post_processor_factory.h"
#include "chromecast/media/cma/backend/mixer/mock_redirected_audio_output.h"
#include "chromecast/media/cma/base/decoder_config_adapter.h"
#include "chromecast/public/media/decoder_config.h"
#include "chromecast/public/media/mixer_output_stream.h"
#include "chromecast/public/volume_control.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"
#include "media/base/channel_layout.h"
#include "media/base/channel_mixer.h"
#include "media/base/vector_math.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::NiceMock;

namespace chromecast {
namespace media {

namespace {

using FloatType = ::media::Float32SampleTypeTraits;
using StreamMatchPatterns =
    std::vector<std::pair<AudioContentType, std::string>>;
using RedirectionConfig = mixer_service::RedirectedAudioConnection::Config;

// Testing constants that are common to multiple test cases.
const size_t kBytesPerSample = sizeof(int32_t);
const int kNumChannels = 2;
const int kOutputFrames = 256;

// kTestSamplesPerSecond needs to be higher than kLowSampleRateCutoff for the
// mixer to use it.
const int kTestSamplesPerSecond = 48000;

// This array holds |NUM_DATA_SETS| sets of arbitrary interleaved float data.
// Each set holds |NUM_SAMPLES| / kNumChannels frames of data.
#define NUM_DATA_SETS 2u
#define NUM_SAMPLES 64u

// Note: Test data should be represented as 32-bit integers and copied into
// ::media::AudioBus instances, rather than wrapping statically declared float
// arrays. The latter method is brittle, as ::media::AudioBus requires 16-byte
// alignment for internal data.
const int32_t kTestData[NUM_DATA_SETS][NUM_SAMPLES] = {
    {
        74343736,    -1333200799, -1360871126, 1138806283,  1931811865,
        1856308487,  649203634,   564640023,   1676630678,  23416591,
        -1293255456, 547928305,   -976258952,  1840550252,  1714525174,
        358704931,   983646295,   1264863573,  442473973,   1222979052,
        317404525,   366912613,   1393280948,  -1022004648, -2054669405,
        -159762261,  1127018745,  -1984491787, 1406988336,  -693327981,
        -1549544744, 1232236854,  970338338,   -1750160519, -783213057,
        1231504562,  1155296810,  -820018779,  1155689800,  -1108462340,
        -150535168,  1033717023,  2121241397,  1829995370,  -1893006836,
        -819097508,  -495186107,  1001768909,  -1441111852, 692174781,
        1916569026,  -687787473,  -910565280,  1695751872,  994166817,
        1775451433,  909418522,   492671403,   -761744663,  -2064315902,
        1357716471,  -1580019684, 1872702377,  -1524457840,
    },
    {
        1951643876,  712069070,   1105286211,  1725522438,  -986938388,
        229538084,   1042753634,  1888456317,  1477803757,  1486284170,
        -340193623,  -1828672521, 1418790906,  -724453609,  -1057163251,
        1408558147,  -31441309,   1421569750,  -1231100836, 545866721,
        1430262764,  2107819625,  -2077050480, -1128358776, -1799818931,
        -1041097926, 1911058583,  -1177896929, -1911123008, -929110948,
        1267464176,  172218310,   -2048128170, -2135590884, 734347065,
        1214930283,  1301338583,  -326962976,  -498269894,  -1167887508,
        -589067650,  591958162,   592999692,   -788367017,  -1389422,
        1466108561,  386162657,   1389031078,  936083827,   -1438801160,
        1340850135,  -1616803932, -850779335,  1666492408,  1290349909,
        -492418001,  659200170,   -542374913,  -120005682,  1030923147,
        -877887021,  -870241979,  1322678128,  -344799975,
    }};

// Compensate for integer arithmetic errors.
const int kMaxDelayErrorUs = 2;

const char kDelayModuleSolib[] = "delay.so";

// Should match # of "processors" blocks below.
const int kNumPostProcessors = 5;
const char kTestPipelineJsonTemplate[] = R"json(
{
  "postprocessors": {
    "output_streams": [{
      "streams": [ "default" ],
      "num_input_channels": 2,
      "processors": [{
        "processor": "%s",
        "config": { "delay": %d }
      }]
    }, {
      "streams": [ "assistant-tts" ],
      "num_input_channels": 2,
      "processors": [{
        "processor": "%s",
        "config": { "delay": %d }
      }]
    }, {
      "streams": [ "communications" ],
      "num_input_channels": 2,
      "processors": []
    }],
    "mix": {
      "num_input_channels": 2,
      "processors": [{
        "processor": "%s",
        "config": { "delay": %d }
       }]
    },
    "linearize": {
      "num_input_channels": 2,
      "processors": [{
        "processor": "%s",
        "config": { "delay": %d }
      }]
    }
  }
}
)json";

const int kDefaultProcessorDelay = 10;
const int kTtsProcessorDelay = 100;
const int kMixProcessorDelay = 1000;
const int kLinearizeProcessorDelay = 10000;

// Return a scoped pointer filled with the data laid out at |index| above.
std::unique_ptr<::media::AudioBus> GetTestData(size_t index) {
  CHECK_LT(index, NUM_DATA_SETS);
  int frames = NUM_SAMPLES / kNumChannels;
  auto data = ::media::AudioBus::Create(kNumChannels, frames);
  data->FromInterleaved<::media::SignedInt32SampleTypeTraits>(kTestData[index],
                                                              frames);
  return data;
}

class MockMixerOutput : public MixerOutputStream {
 public:
  MockMixerOutput() {
    ON_CALL(*this, Start(_, _))
        .WillByDefault(testing::Invoke(this, &MockMixerOutput::StartImpl));
    ON_CALL(*this, GetSampleRate())
        .WillByDefault(
            testing::Invoke(this, &MockMixerOutput::GetSampleRateImpl));
    ON_CALL(*this, GetNumChannels())
        .WillByDefault(
            testing::Invoke(this, &MockMixerOutput::GetNumChannelsImpl));
    ON_CALL(*this, GetRenderingDelay())
        .WillByDefault(
            testing::Invoke(this, &MockMixerOutput::GetRenderingDelayImpl));
    ON_CALL(*this, OptimalWriteFramesCount())
        .WillByDefault(
            testing::Invoke(this, &MockMixerOutput::OptimalWriteFramesImpl));
    ON_CALL(*this, Write(_, _, _))
        .WillByDefault(testing::Invoke(this, &MockMixerOutput::WriteImpl));
  }

  MOCK_METHOD2(Start, bool(int, int));
  MOCK_METHOD0(GetNumChannels, int());
  MOCK_METHOD0(GetSampleRate, int());
  MOCK_METHOD0(GetRenderingDelay,
               MediaPipelineBackend::AudioDecoder::RenderingDelay());
  MOCK_METHOD0(OptimalWriteFramesCount, int());
  MOCK_METHOD3(Write, bool(const float*, int, bool*));
  MOCK_METHOD0(Stop, void());

  int sample_rate() const { return sample_rate_; }
  const std::vector<float>& data() const { return data_; }

  void ClearData() { data_.clear(); }

 private:
  bool StartImpl(int requested_sample_rate, int channels) {
    channels_ = channels;
    sample_rate_ = requested_sample_rate;
    return true;
  }

  int GetNumChannelsImpl() { return channels_; }
  int GetSampleRateImpl() { return sample_rate_; }

  MediaPipelineBackend::AudioDecoder::RenderingDelay GetRenderingDelayImpl() {
    return MediaPipelineBackend::AudioDecoder::RenderingDelay();
  }

  int OptimalWriteFramesImpl() { return kOutputFrames; }

  bool WriteImpl(const float* data,
                 int data_size,
                 bool* out_playback_interrupted) {
    *out_playback_interrupted = false;
    data_.insert(data_.end(), data, data + data_size);
    return true;
  }

  int channels_ = 0;
  int sample_rate_ = 0;
  std::vector<float> data_;
};

#define EXPECT_CALL_ALL_POSTPROCESSORS(factory, call_sig) \
  do {                                                    \
    for (auto& itr : factory->instances) {                \
      EXPECT_CALL(*itr.second, call_sig);                 \
    }                                                     \
  } while (0);

void VerifyAndClearPostProcessors(MockPostProcessorFactory* factory) {
  for (auto& itr : factory->instances) {
    testing::Mock::VerifyAndClearExpectations(itr.second);
  }
}

class MockLoopbackAudioObserver
    : public mixer_service::LoopbackConnection::Delegate {
 public:
  MockLoopbackAudioObserver() = default;

  MockLoopbackAudioObserver(const MockLoopbackAudioObserver&) = delete;
  MockLoopbackAudioObserver& operator=(const MockLoopbackAudioObserver&) =
      delete;

  ~MockLoopbackAudioObserver() override = default;

  MOCK_METHOD6(OnLoopbackAudio,
               void(int64_t, SampleFormat, int, int, uint8_t*, int));
  MOCK_METHOD1(OnLoopbackInterrupted, void(LoopbackInterruptReason));
};

// Given |inputs|, returns mixed audio data according to the mixing method used
// by the mixer.
std::unique_ptr<::media::AudioBus> GetMixedAudioData(
    const std::vector<MockMixerSource*>& inputs,
    bool apply_volume = true) {
  int read_size = 0;
  int num_channels = 1;
  for (auto* input : inputs) {
    CHECK(input);
    read_size = std::max(input->data().frames(), read_size);
    num_channels = std::max(num_channels, input->data().channels());
  }

  // Verify all inputs are the right size.
  for (auto* input : inputs) {
    CHECK_LE(read_size, input->data().frames());
  }

  // Currently, the mixing algorithm is simply to sum the scaled, clipped input
  // streams. Go sample-by-sample and mix the data.
  auto mixed = ::media::AudioBus::Create(num_channels, read_size);
  for (int c = 0; c < mixed->channels(); ++c) {
    for (int f = 0; f < read_size; ++f) {
      float* result = mixed->channel(c) + f;

      // Sum the sample from each input stream, scaling each stream.
      *result = 0.0;
      for (auto* input : inputs) {
        if (input->data().channels() <= c) {
          continue;
        }
        if (input->data().frames() > f) {
          if (apply_volume) {
            *result += *(input->data().channel(c) + f) * input->multiplier();
          } else {
            *result += *(input->data().channel(c) + f);
          }
        }
      }

      *result = std::clamp(*result, -1.0f, 1.0f);
    }
  }
  return mixed;
}

// Like the method above, but accepts a single input. This returns an AudioBus
// with this input after it is scaled and clipped.
std::unique_ptr<::media::AudioBus> GetMixedAudioData(MockMixerSource* input) {
  return GetMixedAudioData(std::vector<MockMixerSource*>(1, input));
}

std::unique_ptr<::media::AudioBus> GetMixedAudioData(
    const std::vector<std::unique_ptr<MockMixerSource>>& inputs) {
  std::vector<MockMixerSource*> ptrs;
  for (const auto& i : inputs) {
    ptrs.push_back(i.get());
  }
  return GetMixedAudioData(ptrs);
}

void ToPlanar(const float* interleaved,
              int num_frames,
              ::media::AudioBus* planar) {
  ASSERT_GE(planar->frames(), num_frames);

  planar->FromInterleaved<FloatType>(interleaved, num_frames);
}

// Asserts that |expected| matches |actual| exactly.
void CompareAudioData(const ::media::AudioBus& expected,
                      const ::media::AudioBus& actual,
                      const std::string& token = "") {
  ASSERT_EQ(expected.channels(), actual.channels());
  ASSERT_EQ(expected.frames(), actual.frames());

  for (int c = 0; c < expected.channels(); ++c) {
    const float* expected_data = expected.channel(c);
    const float* actual_data = actual.channel(c);
    for (int f = 0; f < expected.frames(); ++f) {
      EXPECT_NEAR(*expected_data++, *actual_data++, 0.0000001f)
          << c << " " << f << " " << token;
    }
  }
}

// Check that
// MediaPipelineBackend::AudioDecoder::RenderingDelay.delay_microseconds is
// within kMaxDelayErrorUs of |delay|
MATCHER_P2(MatchDelay, delay, id, "") {
  bool result = std::abs(arg.delay_microseconds - delay) < kMaxDelayErrorUs;
  if (!result) {
    LOG(ERROR) << "Expected delay_microseconds for " << id << " to be " << delay
               << " but got " << arg.delay_microseconds;
  }
  return result;
}

// Convert a number of frames at kTestSamplesPerSecond to microseconds
int64_t FramesToDelayUs(int64_t frames) {
  return frames * base::Time::kMicrosecondsPerSecond / kTestSamplesPerSecond;
}

#if GTEST_HAS_DEATH_TEST

std::string DeathRegex(const std::string& regex) {
// String arguments aren't passed to CHECK() in official builds.
#if BUILDFLAG(IS_ANDROID) || (defined(OFFICIAL_BUILD) && defined(NDEBUG))
  return "";
#else
  return regex;
#endif
}

#endif  // GTEST_HAS_DEATH_TEST

}  // namespace

class StreamMixerTest : public testing::Test {
 public:
  StreamMixerTest(const StreamMixerTest&) = delete;
  StreamMixerTest& operator=(const StreamMixerTest&) = delete;

 protected:
  StreamMixerTest() {
    auto output = std::make_unique<NiceMock<MockMixerOutput>>();
    mock_output_ = output.get();
    mixer_ = std::make_unique<StreamMixer>(
        std::move(output), base::SingleThreadTaskRunner::GetCurrentDefault(),
        "{}");
    mixer_->SetVolume(AudioContentType::kMedia, 1.0f);
    mixer_->SetVolume(AudioContentType::kAlarm, 1.0f);
    std::string test_pipeline_json = base::StringPrintf(
        kTestPipelineJsonTemplate, kDelayModuleSolib, kDefaultProcessorDelay,
        kDelayModuleSolib, kTtsProcessorDelay, kDelayModuleSolib,
        kMixProcessorDelay, kDelayModuleSolib, kLinearizeProcessorDelay);
    auto factory = std::make_unique<MockPostProcessorFactory>();
    pp_factory_ = factory.get();
    mixer_->SetNumOutputChannelsForTest(2);
    mixer_->ResetPostProcessorsForTest(std::move(factory), test_pipeline_json);
    CHECK_EQ(pp_factory_->instances.size(),
             static_cast<size_t>(kNumPostProcessors));
  }

  void WaitForMixer() {
    base::RunLoop run_loop1;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop1.QuitClosure());
    run_loop1.Run();
  }

  void PlaybackOnce() {
    // Run one playback iteration.
    EXPECT_CALL(*mock_output_, Write(_,
                                     mock_output_->OptimalWriteFramesCount() *
                                         mixer_->num_output_channels(),
                                     _))
        .Times(1);
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(mock_output_);
  }

  std::unique_ptr<MockRedirectedAudioOutput> AddOutputRedirector(
      const RedirectionConfig& config,
      StreamMatchPatterns patterns) {
    DCHECK_EQ(config.num_output_channels, kNumChannels);
    auto redirected_output =
        std::make_unique<MockRedirectedAudioOutput>(config);
    redirected_output->SetStreamMatchPatterns(std::move(patterns));
    WaitForMixer();
    return redirected_output;
  }

  void CheckRedirectorOutput(
      MockRedirectedAudioOutput* redirected_output,
      const std::vector<MockMixerSource*>& normal_inputs,
      const std::vector<MockMixerSource*>& redirected_inputs,
      int num_frames,
      bool apply_volume = false) {
    auto actual_mixer_output =
        ::media::AudioBus::Create(kNumChannels, num_frames);
    ASSERT_GE(mock_output_->data().size(), static_cast<size_t>(num_frames));
    ToPlanar(mock_output_->data().data(), num_frames,
             actual_mixer_output.get());
    std::unique_ptr<::media::AudioBus> expected_mixer_output;
    if (normal_inputs.empty()) {
      expected_mixer_output =
          ::media::AudioBus::Create(kNumChannels, num_frames);
      expected_mixer_output->Zero();
    } else {
      expected_mixer_output = GetMixedAudioData(normal_inputs);
    }
    CompareAudioData(*expected_mixer_output, *actual_mixer_output, "output");

    auto actual_redirected_output =
        ::media::AudioBus::Create(kNumChannels, num_frames);
    CHECK(redirected_output->last_buffer());
    ASSERT_GE(redirected_output->last_buffer()->frames(), num_frames);
    redirected_output->last_buffer()->CopyPartialFramesTo(
        0, num_frames, 0, actual_redirected_output.get());
    std::unique_ptr<::media::AudioBus> expected_redirected_output;
    if (redirected_inputs.empty()) {
      expected_redirected_output =
          ::media::AudioBus::Create(kNumChannels, num_frames);
      expected_redirected_output->Zero();
    } else {
      expected_redirected_output =
          GetMixedAudioData(redirected_inputs, apply_volume);
    }
    CompareAudioData(*expected_redirected_output, *actual_redirected_output,
                     "redirected");
  }

  std::unique_ptr<mixer_service::LoopbackConnection> AddLoopbackObserver(
      mixer_service::LoopbackConnection::Delegate* observer) {
    std::unique_ptr<mixer_service::MixerSocket> loopback_socket =
        CreateLoopbackConnectionForTest(mixer_->GetLoopbackHandlerForTest());
    auto connection = std::make_unique<mixer_service::LoopbackConnection>(
        observer, std::move(loopback_socket));
    connection->Connect();
    return connection;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  MockMixerOutput* mock_output_;
  std::unique_ptr<StreamMixer> mixer_;
  MockPostProcessorFactory* pp_factory_;
};

TEST_F(StreamMixerTest, AddSingleInput) {
  MockMixerSource input(kTestSamplesPerSecond);

  EXPECT_CALL(input, InitializeAudioPlayback(_, _)).Times(1);
  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, _)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);
  mixer_->AddInput(&input);
  WaitForMixer();

  mixer_.reset();
}

TEST_F(StreamMixerTest, AddMultipleInputs) {
  MockMixerSource input1(kTestSamplesPerSecond);
  MockMixerSource input2(kTestSamplesPerSecond * 2);

  EXPECT_CALL(input1, InitializeAudioPlayback(_, _)).Times(1);
  EXPECT_CALL(input2, InitializeAudioPlayback(_, _)).Times(1);
  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, _)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);
  mixer_->AddInput(&input1);
  mixer_->AddInput(&input2);
  WaitForMixer();

  mixer_.reset();
}

TEST_F(StreamMixerTest, RemoveInput) {
  std::vector<std::unique_ptr<MockMixerSource>> inputs;
  const int kNumInputs = 3;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(
        std::make_unique<MockMixerSource>(kTestSamplesPerSecond * (i + 1)));
  }

  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, _)).Times(1);

  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], InitializeAudioPlayback(_, _)).Times(1);
    mixer_->AddInput(inputs[i].get());
  }

  WaitForMixer();

  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], FinalizeAudioPlayback()).Times(1);
    mixer_->RemoveInput(inputs[i].get());
  }

  WaitForMixer();
  task_environment_.RunUntilIdle();
}

TEST_F(StreamMixerTest, WriteFrames) {
  std::vector<std::unique_ptr<MockMixerSource>> inputs;
  const int kNumInputs = 3;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(std::make_unique<MockMixerSource>(kTestSamplesPerSecond));
  }

  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, _)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);

  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], InitializeAudioPlayback(_, _)).Times(1);
    mixer_->AddInput(inputs[i].get());
  }

  WaitForMixer();

  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], FillAudioPlaybackFrames(_, _, _)).Times(1);
  }

  PlaybackOnce();

  mixer_.reset();
}

TEST_F(StreamMixerTest, OneStreamMixesProperly) {
  MockMixerSource input(kTestSamplesPerSecond);

  EXPECT_CALL(input, InitializeAudioPlayback(_, _)).Times(1);
  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, _)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);
  mixer_->AddInput(&input);
  WaitForMixer();
  mock_output_->ClearData();

  // Populate the stream with data.
  const int kNumFrames = 32;
  input.SetData(GetTestData(0));

  EXPECT_CALL(input, FillAudioPlaybackFrames(_, _, _)).Times(1);
  PlaybackOnce();

  // Get the actual mixed output, and compare it against the expected stream.
  // The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  ASSERT_GE(mock_output_->data().size(), static_cast<size_t>(kNumFrames));
  ToPlanar(mock_output_->data().data(), kNumFrames, actual.get());
  auto expected = GetMixedAudioData(&input);
  CompareAudioData(*expected, *actual);

  mixer_.reset();
}

TEST_F(StreamMixerTest, OneStreamStereoInput6ChannelOutput) {
  mixer_->SetNumOutputChannelsForTest(6);

  MockMixerSource input(kTestSamplesPerSecond);

  EXPECT_CALL(input, InitializeAudioPlayback(_, _)).Times(1);
  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, 6)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);
  mixer_->AddInput(&input);
  WaitForMixer();
  mock_output_->ClearData();

  // Populate the stream with data.
  const int kNumFrames = 32;
  input.SetData(GetTestData(0));

  EXPECT_CALL(input, FillAudioPlaybackFrames(_, _, _)).Times(1);
  PlaybackOnce();

  // Get the actual mixed output, and compare it against the expected stream.
  // The stream should match exactly.
  auto actual = ::media::AudioBus::Create(6, kNumFrames);
  ASSERT_GE(mock_output_->data().size(), static_cast<size_t>(kNumFrames * 6));
  ToPlanar(mock_output_->data().data(), kNumFrames, actual.get());

  auto expected = GetMixedAudioData(&input);

  // Upmix stereo input to 5.1 before comparing.
  ::media::ChannelLayout input_layout =
      DecoderConfigAdapter::ToMediaChannelLayout(ChannelLayout::STEREO);
  ::media::ChannelLayout output_layout =
      DecoderConfigAdapter::ToMediaChannelLayout(ChannelLayout::SURROUND_5_1);
  ::media::ChannelMixer channel_mixer(
      input_layout, ::media::ChannelLayoutToChannelCount(input_layout),
      output_layout, ::media::ChannelLayoutToChannelCount(output_layout));
  auto expected_5_1 = ::media::AudioBus::Create(6, expected->frames());
  channel_mixer.Transform(expected.get(), expected_5_1.get());
  CompareAudioData(*expected_5_1, *actual);

  mixer_.reset();
}

TEST_F(StreamMixerTest, OneStreamStereoInput6ChannelFilters) {
  const char k6ChannelPostprocessorJson[] = R"json(
{
  "postprocessors": {
    "mix": {
      "streams": [ "default" ],
      "num_input_channels": 6,
      "processors": []
    }
  }
}
)json";

  mixer_->EnableDynamicChannelCountForTest(true);
  mixer_->SetNumOutputChannelsForTest(6);
  auto factory = std::make_unique<MockPostProcessorFactory>();
  mixer_->ResetPostProcessorsForTest(std::move(factory),
                                     k6ChannelPostprocessorJson);

  MockMixerSource input(kTestSamplesPerSecond);

  EXPECT_CALL(input, InitializeAudioPlayback(_, _)).Times(1);
  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, 6)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);
  mixer_->AddInput(&input);
  WaitForMixer();
  mock_output_->ClearData();

  // Populate the stream with data.
  const int kNumFrames = 32;
  input.SetData(GetTestData(0));

  EXPECT_CALL(input, FillAudioPlaybackFrames(_, _, _)).Times(1);
  PlaybackOnce();

  // Get the actual mixed output, and compare it against the expected stream.
  // The stream should match exactly.
  auto actual = ::media::AudioBus::Create(6, kNumFrames);
  ASSERT_GE(mock_output_->data().size(), static_cast<size_t>(kNumFrames * 6));
  ToPlanar(mock_output_->data().data(), kNumFrames, actual.get());

  auto expected = GetMixedAudioData(&input);

  // Upmix stereo input to 5.1 before comparing.
  ::media::ChannelLayout input_layout =
      DecoderConfigAdapter::ToMediaChannelLayout(ChannelLayout::STEREO);
  ::media::ChannelLayout output_layout =
      DecoderConfigAdapter::ToMediaChannelLayout(ChannelLayout::SURROUND_5_1);
  ::media::ChannelMixer channel_mixer(
      input_layout, ::media::ChannelLayoutToChannelCount(input_layout),
      output_layout, ::media::ChannelLayoutToChannelCount(output_layout));
  auto expected_5_1 = ::media::AudioBus::Create(6, expected->frames());
  channel_mixer.Transform(expected.get(), expected_5_1.get());
  CompareAudioData(*expected_5_1, *actual);

  mixer_.reset();
}

TEST_F(StreamMixerTest, OneStreamStereoInput6ChannelFiltersStereoOutput) {
  const char k6ChannelPostprocessorJson[] = R"json(
{
  "postprocessors": {
    "mix": {
      "streams": [ "default" ],
      "num_input_channels": 6,
      "processors": []
    }
  }
}
)json";

  mixer_->EnableDynamicChannelCountForTest(true);
  mixer_->SetNumOutputChannelsForTest(2);
  auto factory = std::make_unique<MockPostProcessorFactory>();
  mixer_->ResetPostProcessorsForTest(std::move(factory),
                                     k6ChannelPostprocessorJson);

  MockMixerSource input(kTestSamplesPerSecond);

  EXPECT_CALL(input, InitializeAudioPlayback(_, _)).Times(1);
  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, 2)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);
  mixer_->AddInput(&input);
  WaitForMixer();
  mock_output_->ClearData();

  // Populate the stream with data.
  const int kNumFrames = 32;
  input.SetData(GetTestData(0));

  EXPECT_CALL(input, FillAudioPlaybackFrames(_, _, _)).Times(1);
  PlaybackOnce();

  // Get the actual mixed output, and compare it against the expected stream.
  // The stream should match exactly.
  auto actual = ::media::AudioBus::Create(2, kNumFrames);
  ASSERT_GE(mock_output_->data().size(), static_cast<size_t>(kNumFrames * 2));
  ToPlanar(mock_output_->data().data(), kNumFrames, actual.get());

  auto expected = GetMixedAudioData(&input);
  CompareAudioData(*expected, *actual);

  mixer_.reset();
}

TEST_F(StreamMixerTest, OneStream10ChannelInputStereoOutput) {
  mixer_->SetNumOutputChannelsForTest(2);

  MockMixerSource input(kTestSamplesPerSecond);
  input.set_num_channels(10);
  input.set_channel_layout(::media::CHANNEL_LAYOUT_DISCRETE);

  EXPECT_CALL(input, InitializeAudioPlayback(_, _)).Times(1);
  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, 2)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);
  mixer_->AddInput(&input);
  WaitForMixer();
  mock_output_->ClearData();

  // Populate the stream with data.
  const int kNumFrames = 32;
  auto data = ::media::AudioBus::Create(10, kNumFrames);
  for (int c = 0; c < 10; ++c) {
    for (int f = 0; f < kNumFrames; ++f) {
      data->channel(c)[f] = (c / 10 + f / kNumFrames) / 10;
    }
  }
  input.SetData(std::move(data));

  EXPECT_CALL(input, FillAudioPlaybackFrames(_, _, _)).Times(1);
  PlaybackOnce();

  // Get the actual mixed output, and compare it against the expected stream.
  // The stream should match exactly.
  auto actual = ::media::AudioBus::Create(2, kNumFrames);
  ASSERT_GE(mock_output_->data().size(), static_cast<size_t>(kNumFrames * 2));
  ToPlanar(mock_output_->data().data(), kNumFrames, actual.get());

  auto expected = GetMixedAudioData(&input);

  // Downmix 10-channel input to stereo before comparing.
  ::media::ChannelMixer channel_mixer(
      mixer::CreateAudioParametersForChannelMixer(
          ::media::CHANNEL_LAYOUT_DISCRETE, 10),
      mixer::CreateAudioParametersForChannelMixer(
          ::media::CHANNEL_LAYOUT_STEREO, 2));
  auto expected_stereo = ::media::AudioBus::Create(2, expected->frames());
  channel_mixer.Transform(expected.get(), expected_stereo.get());
  CompareAudioData(*expected_stereo, *actual);

  mixer_.reset();
}

TEST_F(StreamMixerTest, OneStream10ChannelInputAndOutput) {
  mixer_->SetNumOutputChannelsForTest(10);

  MockMixerSource input(kTestSamplesPerSecond);
  input.set_num_channels(10);
  input.set_channel_layout(::media::CHANNEL_LAYOUT_DISCRETE);

  EXPECT_CALL(input, InitializeAudioPlayback(_, _)).Times(1);
  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, 10)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);
  mixer_->AddInput(&input);
  WaitForMixer();
  mock_output_->ClearData();

  // Populate the stream with data.
  const int kNumFrames = 32;
  auto data = ::media::AudioBus::Create(10, kNumFrames);
  for (int c = 0; c < 10; ++c) {
    for (int f = 0; f < kNumFrames; ++f) {
      data->channel(c)[f] = (c / 10 + f / kNumFrames) / 10;
    }
  }
  input.SetData(std::move(data));

  EXPECT_CALL(input, FillAudioPlaybackFrames(_, _, _)).Times(1);
  PlaybackOnce();

  // Get the actual mixed output, and compare it against the expected stream.
  // The stream should match exactly.
  auto actual = ::media::AudioBus::Create(10, kNumFrames);
  ASSERT_GE(mock_output_->data().size(), static_cast<size_t>(kNumFrames * 10));
  ToPlanar(mock_output_->data().data(), kNumFrames, actual.get());

  auto expected = GetMixedAudioData(&input);
  CompareAudioData(*expected, *actual);

  mixer_.reset();
}

TEST_F(StreamMixerTest, OneStreamIsScaledDownProperly) {
  MockMixerSource input(kTestSamplesPerSecond);

  EXPECT_CALL(input, InitializeAudioPlayback(_, _)).Times(1);
  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, _)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);
  input.set_multiplier(0.75f);
  mixer_->AddInput(&input);
  mixer_->SetVolumeMultiplier(&input, input.multiplier());
  WaitForMixer();

  // Populate the stream with data.
  const int kNumFrames = 32;
  ASSERT_EQ(sizeof(kTestData[0]), kNumChannels * kNumFrames * kBytesPerSample);
  input.SetData(GetTestData(0));

  mock_output_->ClearData();

  EXPECT_CALL(input, FillAudioPlaybackFrames(_, _, _)).Times(1);
  PlaybackOnce();

  // Get the actual mixed output, and compare it against the expected stream.
  // The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  ASSERT_GE(mock_output_->data().size(), static_cast<size_t>(kNumFrames));
  ToPlanar(mock_output_->data().data(), kNumFrames, actual.get());
  auto expected = GetMixedAudioData(&input);
  CompareAudioData(*expected, *actual);

  mixer_.reset();
}

TEST_F(StreamMixerTest, FocusType) {
  MockMixerSource input(kTestSamplesPerSecond);
  input.set_content_type(AudioContentType::kMedia);
  input.set_focus_type(AudioContentType::kCommunication);

  EXPECT_CALL(input, InitializeAudioPlayback(_, _)).Times(1);
  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, _)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);
  mixer_->AddInput(&input);
  mixer_->SetVolume(AudioContentType::kMedia, 0.75);  // This volume is used.
  mixer_->SetOutputLimit(AudioContentType::kMedia, 0.5);  // Limit is not used.
  WaitForMixer();

  // Populate the stream with data.
  const int kNumFrames = 32;
  ASSERT_EQ(sizeof(kTestData[0]), kNumChannels * kNumFrames * kBytesPerSample);
  input.SetData(GetTestData(0));

  mock_output_->ClearData();

  EXPECT_CALL(input, FillAudioPlaybackFrames(_, _, _)).Times(1);
  PlaybackOnce();

  // Get the actual mixed output, and compare it against the expected stream.
  // The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  ASSERT_GE(mock_output_->data().size(), static_cast<size_t>(kNumFrames));
  ToPlanar(mock_output_->data().data(), kNumFrames, actual.get());
  auto expected = GetMixedAudioData(&input);
  expected->Scale(0.75);
  CompareAudioData(*expected, *actual);

  mixer_.reset();
}

TEST_F(StreamMixerTest, TwoUnscaledStreamsMixProperly) {
  // Create a group of input streams.
  std::vector<std::unique_ptr<MockMixerSource>> inputs;
  const int kNumInputs = 2;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(std::make_unique<MockMixerSource>(kTestSamplesPerSecond));
  }

  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, _)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);

  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], InitializeAudioPlayback(_, _)).Times(1);
    mixer_->AddInput(inputs[i].get());
  }
  WaitForMixer();
  mock_output_->ClearData();

  // Populate the streams with data.
  const int kNumFrames = 32;
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
    EXPECT_CALL(*inputs[i], FillAudioPlaybackFrames(_, _, _)).Times(1);
  }

  PlaybackOnce();

  // Get the actual mixed output, and compare it against the expected stream.
  // The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  ASSERT_GE(mock_output_->data().size(), static_cast<size_t>(kNumFrames));
  ToPlanar(mock_output_->data().data(), kNumFrames, actual.get());
  auto expected = GetMixedAudioData(inputs);
  CompareAudioData(*expected, *actual);

  mixer_.reset();
}

TEST_F(StreamMixerTest, TwoUnscaledStreamsWithDifferentIdsMixProperly) {
  // Create a group of input streams.
  std::vector<std::unique_ptr<MockMixerSource>> inputs;
  inputs.push_back(std::make_unique<MockMixerSource>(
      kTestSamplesPerSecond,
      ::media::AudioDeviceDescription::kDefaultDeviceId));
  inputs.push_back(std::make_unique<MockMixerSource>(
      kTestSamplesPerSecond,
      ::media::AudioDeviceDescription::kCommunicationsDeviceId));

  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, _)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);

  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], InitializeAudioPlayback(_, _)).Times(1);
    mixer_->AddInput(inputs[i].get());
  }
  WaitForMixer();
  mock_output_->ClearData();

  // Populate the streams with data.
  const int kNumFrames = 32;
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
    EXPECT_CALL(*inputs[i], FillAudioPlaybackFrames(_, _, _)).Times(1);
  }

  PlaybackOnce();

  // Get the actual mixed output, and compare it against the expected stream.
  // The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  ASSERT_GE(mock_output_->data().size(), static_cast<size_t>(kNumFrames));
  ToPlanar(mock_output_->data().data(), kNumFrames, actual.get());
  auto expected = GetMixedAudioData(inputs);
  CompareAudioData(*expected, *actual);

  mixer_.reset();
}

TEST_F(StreamMixerTest, TwoUnscaledStreamsMixProperlyWithEdgeCases) {
  // Create a group of input streams.
  std::vector<std::unique_ptr<MockMixerSource>> inputs;
  const int kNumInputs = 2;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(std::make_unique<MockMixerSource>(kTestSamplesPerSecond));
  }

  EXPECT_CALL(*mock_output_, Start(kTestSamplesPerSecond, _)).Times(1);
  EXPECT_CALL(*mock_output_, Stop()).Times(0);

  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], InitializeAudioPlayback(_, _)).Times(1);
    mixer_->AddInput(inputs[i].get());
  }
  WaitForMixer();
  mock_output_->ClearData();

  // Create edge case data for the inputs. By mixing these two short streams,
  // every combination of {-(2^31), 0, 2^31-1} is tested. This test case is
  // intended to be a hand-checkable gut check.
  // Note: Test data should be represented as 32-bit integers and copied into
  // ::media::AudioBus instances, rather than wrapping statically declared float
  // arrays. The latter method is brittle, as ::media::AudioBus requires 16-bit
  // alignment for internal data.
  const int kNumFrames = 4;

  const int32_t kMaxSample = std::numeric_limits<int32_t>::max();
  const int32_t kMinSample = std::numeric_limits<int32_t>::min();
  const int32_t kEdgeData[2][8] = {{
                                       kMinSample,
                                       kMinSample,
                                       kMinSample,
                                       0,
                                       0,
                                       kMaxSample,
                                       0,
                                       0,
                                   },
                                   {
                                       kMinSample,
                                       0,
                                       kMaxSample,
                                       0,
                                       kMaxSample,
                                       kMaxSample,
                                       0,
                                       0,
                                   }};

  // Hand-calculate the results. Index 0 is clamped to -(2^31). Index 5 is
  // clamped to 2^31-1.
  const int32_t kResult[8] = {
      kMinSample, kMinSample, 0, 0, kMaxSample, kMaxSample, 0, 0,
  };

  // Populate the streams with data.
  for (size_t i = 0; i < inputs.size(); ++i) {
    auto test_data = ::media::AudioBus::Create(kNumChannels, kNumFrames);
    test_data->FromInterleaved<::media::SignedInt32SampleTypeTraits>(
        kEdgeData[i], kNumFrames);
    inputs[i]->SetData(std::move(test_data));
    EXPECT_CALL(*inputs[i], FillAudioPlaybackFrames(_, _, _)).Times(1);
  }

  PlaybackOnce();

  // Get the actual mixed output, and compare it against the expected stream.
  // The stream should match exactly.
  auto actual = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  ASSERT_GE(mock_output_->data().size(), static_cast<size_t>(kNumFrames));
  ToPlanar(mock_output_->data().data(), kNumFrames, actual.get());

  // Use the hand-calculated results above.
  auto expected = ::media::AudioBus::Create(kNumChannels, kNumFrames);
  expected->FromInterleaved<::media::SignedInt32SampleTypeTraits>(kResult,
                                                                  kNumFrames);

  CompareAudioData(*expected, *actual);

  mixer_.reset();
}

#define EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(map, name, times, frames, \
                                                silence)                  \
  do {                                                                    \
    auto itr = map->find(name);                                           \
    CHECK(itr != map->end()) << "Could not find processor for " << name;  \
    EXPECT_CALL(*(itr->second), ProcessFrames(_, frames, _, _, silence))  \
        .Times(times);                                                    \
  } while (0);

TEST_F(StreamMixerTest, PostProcessorDelayListedDeviceId) {
  int common_delay = kMixProcessorDelay + kLinearizeProcessorDelay;

  std::vector<std::unique_ptr<MockMixerSource>> inputs;
  std::vector<int64_t> delays;
  inputs.push_back(
      std::make_unique<MockMixerSource>(kTestSamplesPerSecond, "default"));
  delays.push_back(common_delay + kDefaultProcessorDelay);

  inputs.push_back(std::make_unique<MockMixerSource>(kTestSamplesPerSecond,
                                                     "communications"));
  delays.push_back(common_delay);

  inputs.push_back(std::make_unique<MockMixerSource>(kTestSamplesPerSecond,
                                                     "assistant-tts"));
  delays.push_back(common_delay + kTtsProcessorDelay);

  // Convert delay from frames to microseconds.
  base::ranges::transform(delays, delays.begin(), &FramesToDelayUs);

  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_CALL(*inputs[i], InitializeAudioPlayback(_, _)).Times(1);
    mixer_->AddInput(inputs[i].get());
  }
  WaitForMixer();

  auto* post_processors = &pp_factory_->instances;
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "default", 1, _,
                                          false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "mix", 1, _, false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "linearize", 1, _,
                                          false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "communications", 1,
                                          _, false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "assistant-tts", 1,
                                          _, false);

  // Poll the inputs for data. Each input will get a different
  // rendering delay based on their device type.
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(0));
    EXPECT_CALL(*inputs[i],
                FillAudioPlaybackFrames(
                    _, MatchDelay(delays[i], inputs[i]->device_id()), _))
        .Times(1);
  }

  PlaybackOnce();

  mixer_.reset();
}

TEST_F(StreamMixerTest, PostProcessorDelayUnlistedDevice) {
  const std::string device_id = "not-a-device-id";
  MockMixerSource input(kTestSamplesPerSecond, device_id);
  input.SetData(GetTestData(0));
  auto* post_processors = &pp_factory_->instances;
  // These will be called once to ensure their buffers are initialized.
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "default",
                                          testing::AtLeast(1), _, _);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "mix",
                                          testing::AtLeast(1), _, _);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "linearize",
                                          testing::AtLeast(1), _, _);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "communications",
                                          testing::AtLeast(1), _, _);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "assistant-tts",
                                          testing::AtLeast(1), _, _);
  mixer_->AddInput(&input);
  WaitForMixer();

  VerifyAndClearPostProcessors(pp_factory_);
  input.SetData(GetTestData(0));

  // Delay should be based on default processor.
  int64_t delay = FramesToDelayUs(
      kDefaultProcessorDelay + kLinearizeProcessorDelay + kMixProcessorDelay);

  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "default", 1, _,
                                          false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "mix", 1, _, false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "linearize", 1, _,
                                          false);

  EXPECT_CALL(input,
              FillAudioPlaybackFrames(_, MatchDelay(delay, device_id), _))
      .Times(1);
  PlaybackOnce();

  mixer_.reset();
}

TEST_F(StreamMixerTest, PostProcessorRingingWithoutInput) {
  const char kTestPipelineJson[] = R"json(
{
  "postprocessors": {
    "output_streams": [{
      "streams": [ "default" ],
      "processors": [{
        "processor": "%s",
        "config": { "delay": 0, "ringing": true}
      }]
    }, {
      "streams": [ "assistant-tts" ],
      "processors": [{
        "processor": "%s",
        "config": { "delay": 0, "ringing": true}
      }]
    }]
  }
}
)json";

  MockMixerSource input(kTestSamplesPerSecond, "default");
  input.SetData(GetTestData(0));

  std::string test_pipeline_json = base::StringPrintf(
      kTestPipelineJson, kDelayModuleSolib, kDelayModuleSolib);
  auto factory = std::make_unique<MockPostProcessorFactory>();
  MockPostProcessorFactory* factory_ptr = factory.get();
  mixer_->ResetPostProcessorsForTest(std::move(factory), test_pipeline_json);
  mixer_->AddInput(&input);
  WaitForMixer();

  // "mix" + "linearize" should be automatic
  EXPECT_GE(factory_ptr->instances.size(), 4u);

  auto* post_processors = &factory_ptr->instances;
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "default", 1, _, _);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "mix", 1, _, false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "linearize", 1, _,
                                          false);
  EXPECT_POSTPROCESSOR_CALL_PROCESSFRAMES(post_processors, "assistant-tts", 1,
                                          _, true);

  PlaybackOnce();
  mixer_.reset();
}

TEST_F(StreamMixerTest, PostProcessorProvidesDefaultPipeline) {
  auto factory = std::make_unique<MockPostProcessorFactory>();
  MockPostProcessorFactory* factory_ptr = factory.get();
  mixer_->ResetPostProcessorsForTest(std::move(factory), "{}");

  auto* instances = &factory_ptr->instances;
  CHECK(instances->find("default") == instances->end());
  CHECK(instances->find("mix") != instances->end());
  CHECK(instances->find("linearize") != instances->end());
  CHECK_EQ(instances->size(), 2u);
}

TEST_F(StreamMixerTest, MultiplePostProcessorsInOneStream) {
  const char kJsonTemplate[] = R"json(
{
  "postprocessors": {
    "output_streams": [{
      "streams": [ "default" ],
      "processors": [{
        "processor": "%s",
        "name": "%s",
        "config": { "delay": 10 }
      }, {
        "processor": "%s",
        "name": "%s",
        "config": { "delay": 100 }
      }]
    }],
    "mix": {
      "processors": [{
        "processor": "%s",
        "name": "%s",
        "config": { "delay": 1000 }
      }, {
        "processor": "%s",
        "config": { "delay": 10000 }
      }]
    }
  }
}
)json";

  std::string json = base::StringPrintf(
      kJsonTemplate, kDelayModuleSolib, "delayer_1",  // unique processor name
      kDelayModuleSolib, "delayer_2",  // non-unique processor names
      kDelayModuleSolib, "delayer_2",
      kDelayModuleSolib  // intentionally omitted processor name
  );

  auto factory = std::make_unique<MockPostProcessorFactory>();
  MockPostProcessorFactory* factory_ptr = factory.get();
  mixer_->ResetPostProcessorsForTest(std::move(factory), json);

  // "mix" + "linearize" + "default"
  EXPECT_EQ(factory_ptr->instances.size(), 3u);

  auto* post_processors = &factory_ptr->instances;
  EXPECT_EQ(post_processors->find("default")->second->delay(), 110);
  EXPECT_EQ(post_processors->find("mix")->second->delay(), 11000);
  EXPECT_EQ(post_processors->find("linearize")->second->delay(), 0);
}

TEST_F(StreamMixerTest, SetPostProcessorConfig) {
  std::string name = "ThisIsMyName";
  std::string config = "ThisIsMyConfig";

  for (auto const& x : pp_factory_->instances) {
    EXPECT_CALL(*(x.second), SetPostProcessorConfig(name, config));
  }

  mixer_->SetPostProcessorConfig(name, config);
  WaitForMixer();
}

TEST_F(StreamMixerTest, ObserverGets2ChannelsByDefault) {
  MockMixerSource input(kTestSamplesPerSecond);
  testing::StrictMock<MockLoopbackAudioObserver> observer;
  mixer_->AddInput(&input);
  auto connection = AddLoopbackObserver(&observer);
  EXPECT_CALL(observer,
              OnLoopbackAudio(_, kSampleFormatF32, kTestSamplesPerSecond,
                              kNumChannels, _, _))
      .Times(testing::AtLeast(1));

  WaitForMixer();
  PlaybackOnce();

  WaitForMixer();
  mixer_.reset();
}

TEST_F(StreamMixerTest, ObserverGets1ChannelIfNumOutputChannelsIs1) {
  const int kNumOutputChannels = 1;
  mixer_->SetNumOutputChannelsForTest(kNumOutputChannels);

  MockMixerSource input(kTestSamplesPerSecond);
  testing::StrictMock<MockLoopbackAudioObserver> observer;
  mixer_->AddInput(&input);
  auto connection = AddLoopbackObserver(&observer);

  EXPECT_CALL(observer,
              OnLoopbackAudio(_, kSampleFormatF32, kTestSamplesPerSecond,
                              kNumOutputChannels, _, _))
      .Times(testing::AtLeast(1));

  WaitForMixer();
  PlaybackOnce();

  WaitForMixer();
  mixer_.reset();
}

TEST_F(StreamMixerTest, OneStreamOutputRedirection) {
  RedirectionConfig config;
  StreamMatchPatterns patterns = {{AudioContentType::kMedia, "*"}};
  std::unique_ptr<MockRedirectedAudioOutput> redirected_output =
      AddOutputRedirector(config, patterns);
  WaitForMixer();

  MockMixerSource input(kTestSamplesPerSecond);
  mixer_->AddInput(&input);
  WaitForMixer();
  mock_output_->ClearData();

  // Populate the stream with data.
  const int kNumFrames = 32;
  input.SetData(GetTestData(0));

  testing::Expectation set_sample_rate_is_called =
      EXPECT_CALL(*redirected_output, SetSampleRate(kTestSamplesPerSecond))
          .Times(testing::AtLeast(1));
  EXPECT_CALL(*redirected_output, OnRedirectedAudio(_, _, kOutputFrames))
      .Times(2)
      .After(set_sample_rate_is_called);
  PlaybackOnce();  // First buffer is faded in, so don't try to compare it.
  input.SetData(GetTestData(0));
  PlaybackOnce();
  // Need to wait 2 extra times since we post a task on the mixer side (to the
  // IO task runner) and again to actually "send" the data.
  WaitForMixer();
  WaitForMixer();

  CheckRedirectorOutput(redirected_output.get(), {}, {&input}, kNumFrames);

  mixer_.reset();
}

TEST_F(StreamMixerTest, OutputRedirectionOrder) {
  RedirectionConfig config;
  StreamMatchPatterns patterns = {{AudioContentType::kMedia, "*"}};
  std::unique_ptr<MockRedirectedAudioOutput> redirected_output1 =
      AddOutputRedirector(config, patterns);

  config.order = 1;
  std::unique_ptr<MockRedirectedAudioOutput> redirected_output2 =
      AddOutputRedirector(config, patterns);
  WaitForMixer();

  MockMixerSource input(kTestSamplesPerSecond);
  mixer_->AddInput(&input);
  WaitForMixer();
  mock_output_->ClearData();

  // Populate the stream with data.
  const int kNumFrames = 32;
  input.SetData(GetTestData(0));

  EXPECT_CALL(*redirected_output1, OnRedirectedAudio(_, _, kOutputFrames))
      .Times(2);
  EXPECT_CALL(*redirected_output2, OnRedirectedAudio(_, _, kOutputFrames))
      .Times(0);
  PlaybackOnce();  // First buffer is faded in, so don't try to compare it.
  input.SetData(GetTestData(0));
  PlaybackOnce();
  WaitForMixer();
  WaitForMixer();

  // First redirector should produce actual data, since it has a lower order.
  CheckRedirectorOutput(redirected_output1.get(), {}, {&input}, kNumFrames);
  // Second redirector should not have received any data.
  EXPECT_FALSE(redirected_output2->last_buffer());

  mixer_.reset();
}

TEST_F(StreamMixerTest, TwoStreamsOutputRedirection) {
  std::vector<std::unique_ptr<MockMixerSource>> inputs;
  const int kNumInputs = 2;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(std::make_unique<MockMixerSource>(kTestSamplesPerSecond));
  }

  for (size_t i = 0; i < inputs.size(); ++i) {
    mixer_->AddInput(inputs[i].get());
  }
  WaitForMixer();
  mock_output_->ClearData();

  RedirectionConfig config;
  StreamMatchPatterns patterns = {{AudioContentType::kMedia, "*"}};
  std::unique_ptr<MockRedirectedAudioOutput> redirected_output =
      AddOutputRedirector(config, patterns);
  WaitForMixer();

  const int kNumFrames = 32;
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  WaitForMixer();
  WaitForMixer();

  CheckRedirectorOutput(redirected_output.get(), {},
                        {inputs[0].get(), inputs[1].get()}, kNumFrames);

  mixer_.reset();
}

TEST_F(StreamMixerTest, TwoStreamsOutputRedirectionWithVolume) {
  std::vector<std::unique_ptr<MockMixerSource>> inputs;
  const int kNumInputs = 2;
  for (int i = 0; i < kNumInputs; ++i) {
    inputs.push_back(std::make_unique<MockMixerSource>(kTestSamplesPerSecond));
  }
  inputs[0]->set_multiplier(0.5);
  inputs[1]->set_multiplier(0.7);

  for (size_t i = 0; i < inputs.size(); ++i) {
    mixer_->AddInput(inputs[i].get());
    mixer_->SetVolumeMultiplier(inputs[i].get(), inputs[i]->multiplier());
  }
  WaitForMixer();
  mock_output_->ClearData();

  RedirectionConfig config;
  config.apply_volume = true;
  StreamMatchPatterns patterns = {{AudioContentType::kMedia, "*"}};
  std::unique_ptr<MockRedirectedAudioOutput> redirected_output =
      AddOutputRedirector(config, patterns);
  WaitForMixer();

  const int kNumFrames = 32;
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  WaitForMixer();
  WaitForMixer();

  CheckRedirectorOutput(redirected_output.get(), {},
                        {inputs[0].get(), inputs[1].get()}, kNumFrames,
                        true /* apply_volume */);

  mixer_.reset();
}

TEST_F(StreamMixerTest, OutputRedirectionMatchDeviceId) {
  std::vector<std::unique_ptr<MockMixerSource>> inputs;
  inputs.push_back(
      std::make_unique<MockMixerSource>(kTestSamplesPerSecond, "matches"));
  inputs.push_back(
      std::make_unique<MockMixerSource>(kTestSamplesPerSecond, "asdf"));

  for (size_t i = 0; i < inputs.size(); ++i) {
    mixer_->AddInput(inputs[i].get());
  }
  WaitForMixer();
  mock_output_->ClearData();

  RedirectionConfig config;
  StreamMatchPatterns patterns = {{AudioContentType::kMedia, "*match*"}};
  std::unique_ptr<MockRedirectedAudioOutput> redirected_output =
      AddOutputRedirector(config, patterns);
  WaitForMixer();

  const int kNumFrames = 32;
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  mock_output_->ClearData();
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  WaitForMixer();
  WaitForMixer();

  CheckRedirectorOutput(redirected_output.get(), {inputs[1].get()},
                        {inputs[0].get()}, kNumFrames);

  mixer_.reset();
}

TEST_F(StreamMixerTest, OutputRedirectionMatchContentType) {
  std::vector<std::unique_ptr<MockMixerSource>> inputs;
  inputs.push_back(
      std::make_unique<MockMixerSource>(kTestSamplesPerSecond, "matches1"));
  inputs.push_back(
      std::make_unique<MockMixerSource>(kTestSamplesPerSecond, "matches2"));
  inputs[0]->set_content_type(AudioContentType::kAlarm);

  for (size_t i = 0; i < inputs.size(); ++i) {
    mixer_->AddInput(inputs[i].get());
  }
  WaitForMixer();
  mock_output_->ClearData();

  RedirectionConfig config;
  StreamMatchPatterns patterns = {{AudioContentType::kAlarm, "*match*"}};
  std::unique_ptr<MockRedirectedAudioOutput> redirected_output =
      AddOutputRedirector(config, patterns);
  WaitForMixer();

  const int kNumFrames = 32;
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  mock_output_->ClearData();
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  WaitForMixer();
  WaitForMixer();

  CheckRedirectorOutput(redirected_output.get(), {inputs[1].get()},
                        {inputs[0].get()}, kNumFrames);

  mixer_.reset();
}

TEST_F(StreamMixerTest, OutputRedirectionNoMatch) {
  std::vector<std::unique_ptr<MockMixerSource>> inputs;
  inputs.push_back(std::make_unique<MockMixerSource>(kTestSamplesPerSecond));
  inputs.push_back(std::make_unique<MockMixerSource>(kTestSamplesPerSecond));

  for (size_t i = 0; i < inputs.size(); ++i) {
    mixer_->AddInput(inputs[i].get());
  }
  WaitForMixer();
  mock_output_->ClearData();

  RedirectionConfig config;
  StreamMatchPatterns patterns = {{AudioContentType::kAlarm, "*"}};
  std::unique_ptr<MockRedirectedAudioOutput> redirected_output =
      AddOutputRedirector(config, patterns);
  WaitForMixer();

  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  mock_output_->ClearData();
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  WaitForMixer();
  WaitForMixer();

  // Redirector should not have received any data, since no inputs match.
  EXPECT_FALSE(redirected_output->last_buffer());

  mixer_.reset();
}

TEST_F(StreamMixerTest, ModifyOutputRedirection) {
  std::vector<std::unique_ptr<MockMixerSource>> inputs;
  inputs.push_back(
      std::make_unique<MockMixerSource>(kTestSamplesPerSecond, "matches"));
  inputs.push_back(
      std::make_unique<MockMixerSource>(kTestSamplesPerSecond, "asdf"));

  for (size_t i = 0; i < inputs.size(); ++i) {
    mixer_->AddInput(inputs[i].get());
  }
  WaitForMixer();
  mock_output_->ClearData();

  RedirectionConfig config;
  StreamMatchPatterns patterns = {{AudioContentType::kMedia, "*match*"}};
  std::unique_ptr<MockRedirectedAudioOutput> redirected_output =
      AddOutputRedirector(config, patterns);
  WaitForMixer();

  const int kNumFrames = 32;
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  mock_output_->ClearData();
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  WaitForMixer();
  WaitForMixer();

  CheckRedirectorOutput(redirected_output.get(), {inputs[1].get()},
                        {inputs[0].get()}, kNumFrames);

  StreamMatchPatterns new_patterns = {{AudioContentType::kMedia, "*asdf*"}};
  redirected_output->SetStreamMatchPatterns(new_patterns);
  WaitForMixer();
  WaitForMixer();
  WaitForMixer();
  mock_output_->ClearData();

  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  mock_output_->ClearData();
  for (size_t i = 0; i < inputs.size(); ++i) {
    inputs[i]->SetData(GetTestData(i));
  }
  PlaybackOnce();
  WaitForMixer();
  WaitForMixer();

  CheckRedirectorOutput(redirected_output.get(), {inputs[0].get()},
                        {inputs[1].get()}, kNumFrames);

  mixer_.reset();
}

#if GTEST_HAS_DEATH_TEST

using StreamMixerDeathTest = StreamMixerTest;

TEST_F(StreamMixerDeathTest, InvalidStreamTypeCrashes) {
  const char json[] = R"json(
{
  "postprocessors": {
    "output_streams": [{
      "streams": [ "foobar" ],
      "processors": [{
        "processor": "dont_care.so",
        "config": { "delay": 0 }
      }]
    }]
  }
}
)json";

  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(mixer_->ResetPostProcessorsForTest(
                   std::make_unique<MockPostProcessorFactory>(), json),
               DeathRegex("foobar is not a stream type"));
}

TEST_F(StreamMixerDeathTest, BadJsonCrashes) {
  const std::string json("{{");
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(mixer_->ResetPostProcessorsForTest(
                   std::make_unique<MockPostProcessorFactory>(), json),
               DeathRegex("Invalid JSON"));
}

#endif  // GTEST_HAS_DEATH_TEST

}  // namespace media
}  // namespace chromecast
