// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A standalone utility for processing audio files through the cast
// MixerPipeline. This can be used to pre-process files for offline testing.

#include <unistd.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/numerics/ranges.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "chromecast/media/cma/backend/cast_audio_json.h"
#include "chromecast/media/cma/backend/cplay/wav_header.h"
#include "chromecast/media/cma/backend/mixer/mixer_input.h"
#include "chromecast/media/cma/backend/mixer/mixer_pipeline.h"
#include "chromecast/media/cma/backend/mixer/post_processing_pipeline_impl.h"
#include "chromecast/media/cma/backend/mixer/post_processing_pipeline_parser.h"
#include "chromecast/media/cma/backend/volume_map.h"
#include "chromecast/public/media/media_pipeline_backend.h"
#include "media/audio/wav_audio_handler.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_sample_types.h"

namespace chromecast {
namespace media {

// Need an implementation of DbFSToVolume, but volume_control.cc pulls in too
// too many dependencies.
VolumeMap& GetVolumeMap() {
  static base::NoDestructor<VolumeMap> volume_map;
  return *volume_map;
}

namespace {

const int kReadSize = 1024;

void PrintHelp(const std::string& command) {
  LOG(INFO) << "Usage: " << command;
  LOG(INFO) << "  -i input .wav file";
  LOG(INFO) << "  -o output .wav file";
  LOG(INFO) << "  -r output samples per second";
  LOG(INFO) << " [-c cast_audio.json path]";
  LOG(INFO) << " [-v cast volume as fraction of 1 (0.0-1.0)]";
  LOG(INFO) << " [-d max duration (s)]";
}

struct Parameters {
  double cast_volume = 1.0;
  double duration_s = std::numeric_limits<double>::infinity();
  int output_samples_per_second = -1;
  std::string device_id = "default";
  base::FilePath input_file_path;
  base::FilePath output_file_path;
  base::FilePath cast_audio_json_path;
};

std::string ReadInputFile(const Parameters& params) {
  if (!base::PathExists(params.input_file_path)) {
    NOTREACHED() << "File " << params.input_file_path << " does not exist.";
  }
  std::string wav_data;
  if (!base::ReadFileToString(params.input_file_path, &wav_data)) {
    NOTREACHED() << "Unable to open wav file, " << params.input_file_path;
  }
  return wav_data;
}

// MixerInput::Source that reads from a .wav file.
class WavMixerInputSource : public MixerInput::Source {
 public:
  WavMixerInputSource(const Parameters& params)
      : wav_data_(ReadInputFile(params)),
        input_handler_(::media::WavAudioHandler::Create(wav_data_)),
        device_id_(params.device_id),
        bytes_per_frame_(input_handler_->num_channels() *
                         input_handler_->bits_per_sample() / 8) {
    DCHECK(input_handler_);
    LOG(INFO) << "Loaded " << params.input_file_path << ":";
    LOG(INFO) << " Channels: " << input_handler_->num_channels()
              << " Bit Depth: " << input_handler_->bits_per_sample()
              << " Duration: " << input_handler_->GetDuration().InSecondsF()
              << "s.";
  }

  ~WavMixerInputSource() override = default;

  bool AtEnd() { return input_handler_->AtEnd(cursor_); }

  // MixerInput::Source implementation:
  int num_channels() override { return input_handler_->num_channels(); }
  int input_samples_per_second() override {
    return input_handler_->sample_rate();
  }
  bool primary() override { return true; }
  bool active() override { return true; }
  const std::string& device_id() override { return device_id_; }
  AudioContentType content_type() override { return AudioContentType::kMedia; }
  int desired_read_size() override { return kReadSize; }
  int playout_channel() override { return -1; }

  void InitializeAudioPlayback(
      int read_size,
      MixerInput::RenderingDelay initial_rendering_delay) override {}

  int FillAudioPlaybackFrames(int num_frames,
                              MixerInput::RenderingDelay rendering_delay,
                              ::media::AudioBus* buffer) override {
    CHECK(buffer);
    size_t bytes_written;
    CHECK_EQ(num_frames, buffer->frames());
    CHECK(input_handler_->CopyTo(buffer, cursor_, &bytes_written));
    cursor_ += bytes_written;
    return bytes_written / bytes_per_frame_;
  }

  void OnAudioPlaybackError(MixerError error) override {}
  void FinalizeAudioPlayback() override {}

 private:
  const std::string wav_data_;
  std::unique_ptr<::media::WavAudioHandler> input_handler_;
  size_t cursor_ = 0;
  const std::string device_id_;
  const size_t bytes_per_frame_;

  DISALLOW_COPY_AND_ASSIGN(WavMixerInputSource);
};

// OutputHandler interface to allow switching between alsa and file output.
// TODO(bshaya): Add option to play directly to an output device!
class OutputHandler {
 public:
  virtual ~OutputHandler() = default;
  virtual void WriteData(float* data, int num_frames) = 0;
};

class WavOutputHandler : public OutputHandler {
 public:
  WavOutputHandler(const base::FilePath& path,
                   int num_channels,
                   int sample_rate)
      : wav_file_(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE),
        num_channels_(num_channels) {
    header_.SetNumChannels(num_channels);
    header_.SetSampleRate(sample_rate);

    // Write wav file header to fill space. We'll need to go back and fill in
    // the size later.
    wav_file_.WriteAtCurrentPos(reinterpret_cast<char*>(&header_),
                                sizeof(header_));
  }

  ~WavOutputHandler() override {
    // Update size and re-write header. We only really need to overwrite 8 bytes
    // but it is easy and cheap to overwrite the whole header.
    int size = wav_file_.GetLength();
    int data_size = size - sizeof(header_);
    header_.SetDataSize(data_size);
    wav_file_.Write(0 /* offset */, reinterpret_cast<char*>(&header_),
                    sizeof(header_));
  }

  void WriteData(float* data, int num_frames) override {
    std::vector<float> clipped_data(num_frames * num_channels_);
    std::memcpy(clipped_data.data(), data,
                clipped_data.size() * sizeof(clipped_data[0]));
    for (size_t i = 0; i < clipped_data.size(); ++i) {
      clipped_data[i] = base::ClampToRange(clipped_data[i], -1.0f, 1.0f);
    }
    wav_file_.WriteAtCurrentPos(reinterpret_cast<char*>(clipped_data.data()),
                                sizeof(clipped_data[0]) * clipped_data.size());
  }

 private:
  WavHeader header_;
  base::File wav_file_;
  const int num_channels_;

  DISALLOW_COPY_AND_ASSIGN(WavOutputHandler);
};

class AudioMetrics {
 public:
  AudioMetrics(int num_channels) : num_channels_(num_channels) {}
  ~AudioMetrics() = default;

  void ProcessFrames(float* data, int num_frames) {
    for (int i = 0; i < num_frames * num_channels_; ++i) {
      squared_sum_ += std::pow(data[i], 2);
      if (std::fabs(data[i]) > 1.0) {
        ++clipped_samples_;
        largest_sample_ = std::max(largest_sample_, std::fabs(data[i]));
      }
    }
    total_samples_ += num_frames * num_channels_;
  }

  void PrintReport() {
    LOG(INFO) << "RMS magnitude: "
              << 20 * log10(std::sqrt(squared_sum_ / total_samples_)) << "dB";
    if (clipped_samples_ == 0) {
      return;
    }
    LOG(WARNING) << "Detected " << clipped_samples_ << " clipped samples ("
                 << 100.0 * clipped_samples_ / total_samples_ << "%).";
    LOG(WARNING) << "Largest sample: " << largest_sample_;
  }

 private:
  const int num_channels_;
  int clipped_samples_ = 0;
  int total_samples_ = 0;
  float largest_sample_ = 0.0f;
  double squared_sum_ = 0.0;

  DISALLOW_COPY_AND_ASSIGN(AudioMetrics);
};

Parameters ReadArgs(int argc, char* argv[]) {
  Parameters params;
  params.cast_audio_json_path = CastAudioJson::GetFilePath();
  int opt;
  while ((opt = getopt(argc, argv, "i:o:c:v:d:r:")) != -1) {
    switch (opt) {
      case 'i':
        params.input_file_path = base::FilePath(optarg);
        break;
      case 'o':
        params.output_file_path = base::FilePath(optarg);
        break;
      case 'c':
        params.cast_audio_json_path = base::FilePath(optarg);
        break;
      case 'v':
        params.cast_volume = strtod(optarg, nullptr);
        CHECK_LE(params.cast_volume, 1.0);
        CHECK_GE(params.cast_volume, 0.0);
        break;
      case 'd':
        params.duration_s = strtod(optarg, nullptr);
        break;
      case 'r':
        params.output_samples_per_second = strtod(optarg, nullptr);
        break;
      default:
        PrintHelp(argv[0]);
        exit(1);
    }
  }
  if (params.input_file_path.empty()) {
    PrintHelp(argv[0]);
    exit(1);
  }
  if (params.output_file_path.empty()) {
    PrintHelp(argv[0]);
    exit(1);
  }
  return params;
}

// Real main() inside namespace.
int CplayMain(int argc, char* argv[]) {
  Parameters params = ReadArgs(argc, argv);

  // Comply with CastAudioJsonProviderImpl.
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "cplay_thread_pool");

  // Read input file.
  WavMixerInputSource input_source(params);
  if (params.output_samples_per_second <= 0) {
    params.output_samples_per_second = input_source.input_samples_per_second();
  }
  if (params.output_samples_per_second !=
      input_source.input_samples_per_second()) {
    LOG(INFO) << "Resampling from " << input_source.input_samples_per_second()
              << " to " << params.output_samples_per_second;
  } else {
    LOG(INFO) << "Sample rate: " << params.output_samples_per_second;
  }

  // Build Processing Pipeline.
  PostProcessingPipelineParser parser(params.cast_audio_json_path);
  auto factory = std::make_unique<PostProcessingPipelineFactoryImpl>();
  auto pipeline = MixerPipeline::CreateMixerPipeline(
      &parser, factory.get(), input_source.num_channels());
  CHECK(pipeline);
  pipeline->Initialize(params.output_samples_per_second, kReadSize);
  LOG(INFO) << "Initialized Cast Audio Pipeline at "
            << params.output_samples_per_second << " samples per second";

  // Add |input_source| to |pipeline|
  FilterGroup* input_group = pipeline->GetInputGroup(params.device_id);
  CHECK(input_group);
  MixerInput mixer_input(&input_source, input_group);

  // Set volume.
  std::string contents;
  base::ReadFileToString(params.cast_audio_json_path, &contents);
  GetVolumeMap().LoadVolumeMap(base::JSONReader::ReadDeprecated(contents));
  float volume_dbfs = GetVolumeMap().VolumeToDbFS(params.cast_volume);
  float volume_multiplier = std::pow(10.0, volume_dbfs / 20.0);
  mixer_input.SetVolumeMultiplier(1.0);
  mixer_input.SetContentTypeVolume(volume_multiplier, 0 /* fade_ms */);
  LOG(INFO) << "Volume set to level " << params.cast_volume << " | "
            << volume_dbfs << "dBFS | multiplier=" << volume_multiplier;

  // Prepare output.
  std::unique_ptr<OutputHandler> output_handler_ =
      std::make_unique<WavOutputHandler>(params.output_file_path,
                                         pipeline->GetOutputChannelCount(),
                                         params.output_samples_per_second);
  AudioMetrics audio_metrics(pipeline->GetOutputChannelCount());

  // Play!
  int frames_written = 0;
  while (!input_source.AtEnd() &&
         frames_written / params.output_samples_per_second <
             params.duration_s) {
    pipeline->MixAndFilter(kReadSize, MixerInput::RenderingDelay());
    audio_metrics.ProcessFrames(pipeline->GetOutput(), kReadSize);
    output_handler_->WriteData(pipeline->GetOutput(), kReadSize);
    frames_written += kReadSize;
  }

  audio_metrics.PrintReport();
  base::ThreadPoolInstance::Get()->Shutdown();
  return 0;
}

}  // namespace
}  // namespace media
}  // namespace chromecast

int main(int argc, char* argv[]) {
  return chromecast::media::CplayMain(argc, argv);
}
