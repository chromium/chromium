// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/post_processing_pipeline_impl.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/scoped_native_library.h"
#include "base/values.h"
#include "chromecast/public/media/audio_post_processor2_shlib.h"
#include "chromecast/public/volume_control.h"

namespace chromecast {
namespace media {

namespace {

// Used for AudioPostProcessor(1)
const char kJsonKeyProcessor[] = "processor";

// Used for AudioPostProcessor2
const char kJsonKeyLib[] = "lib";

const char kJsonKeyName[] = "name";
const char kJsonKeyConfig[] = "config";

}  // namespace

PostProcessingPipelineFactoryImpl::PostProcessingPipelineFactoryImpl() =
    default;
PostProcessingPipelineFactoryImpl::~PostProcessingPipelineFactoryImpl() =
    default;

std::unique_ptr<PostProcessingPipeline>
PostProcessingPipelineFactoryImpl::CreatePipeline(
    const std::string& name,
    const base::Value* filter_description_list,
    int num_channels) {
  return std::make_unique<PostProcessingPipelineImpl>(
      name, filter_description_list, num_channels);
}

PostProcessingPipelineImpl::PostProcessingPipelineImpl(
    const std::string& name,
    const base::Value* filter_description_list,
    int channels)
    : name_(name), num_output_channels_(channels) {
  if (!filter_description_list) {
    return;  // Warning logged.
  }

  LOG(INFO) << "Create pipeline for " << channels << " input channels";
  for (const base::Value& processor_description_value :
       filter_description_list->GetList()) {
    DCHECK(processor_description_value.is_dict());
    const base::Value::Dict& processor_description_dict =
        processor_description_value.GetDict();

    std::string processor_name;
    const std::string* name_val =
        processor_description_dict.FindString(kJsonKeyName);
    if (name_val) {
      processor_name = *name_val;
    }

    if (!processor_name.empty()) {
      std::vector<PostProcessorInfo>::iterator it =
          find_if(processors_.begin(), processors_.end(),
                  [&processor_name](PostProcessorInfo& p) {
                    return p.name == processor_name;
                  });
      LOG_IF(DFATAL, it != processors_.end())
          << "Duplicate postprocessor name " << processor_name;
    }

    std::string library_path;

    // Keys for AudioPostProcessor2:
    const std::string* library_val =
        processor_description_dict.FindString(kJsonKeyLib);
    if (library_val) {
      library_path = *library_val;
    } else {
      // Keys for AudioPostProcessor
      // TODO(bshaya): Remove when AudioPostProcessor support is removed.
      library_val = processor_description_dict.FindString(kJsonKeyProcessor);
      DCHECK(library_val) << "Post processor description is missing key "
                          << kJsonKeyLib;
      library_path = *library_val;
    }

    std::string processor_config_string;
    const base::Value* processor_config_val =
        processor_description_dict.Find(kJsonKeyConfig);
    if (processor_config_val) {
      DCHECK(processor_config_val->is_dict() ||
             processor_config_val->is_string());
      base::JSONWriter::Write(*processor_config_val, &processor_config_string);
    }

    LOG(INFO) << "Creating '" << processor_name << "', an instance of "
              << library_path << ", for " << channels << " channels";

    processors_.emplace_back(PostProcessorInfo{
        factory_.CreatePostProcessor(library_path, processor_config_string,
                                     channels),
        1 /* output_frames_per_input_frame */, processor_name});
    channels = processors_.back().ptr->GetStatus().output_channels;
  }
  num_output_channels_ = channels;
  LOG(INFO) << "Created pipeline with " << channels << " output channels";
}

PostProcessingPipelineImpl::~PostProcessingPipelineImpl() = default;

void PostProcessingPipelineImpl::ProcessFrames(float* data,
                                               int num_input_frames,
                                               float current_multiplier,
                                               float target_multiplier,
                                               bool is_silence) {
  DCHECK_GT(input_sample_rate_, 0);
  DCHECK(data);
  if (processors_.size() > 0) {
    DCHECK_EQ(processors_[0].input_frames_per_write, num_input_frames);
  }

  output_buffer_ = data;

  if (is_silence) {
    if (!IsRinging()) {
      // If the input sample rate differs from the output sample rate, then the
      // input data will be the incorrect size without any resampling. Output a
      // zeroed buffer of correct size.
      if (input_sample_rate_ != output_sample_rate_) {
        // We cannot guarantee that the consumer of the output buffer will not
        // mutate it, so set it back to zero.
        std::fill_n(silence_buffer_.data(), silence_buffer_.size(), 0.0);
        output_buffer_ = silence_buffer_.data();
      }
      return;
    }
    silence_frames_processed_ += num_input_frames;
  } else {
    silence_frames_processed_ = 0;
  }

  UpdateCastVolume(current_multiplier, target_multiplier);
  AudioPostProcessor2::Metadata metadata = {current_dbfs_, target_dbfs_,
                                            cast_volume_};

  delay_s_ = 0;
  for (auto& processor : processors_) {
    processor.ptr->ProcessFrames(output_buffer_,
                                 processor.input_frames_per_write, &metadata);
    const auto& status = processor.ptr->GetStatus();
    delay_s_ += static_cast<double>(status.rendering_delay_frames) /
                status.input_sample_rate;
    output_buffer_ = status.output_buffer;
  }
}

int PostProcessingPipelineImpl::NumOutputChannels() const {
  return num_output_channels_;
}

float* PostProcessingPipelineImpl::GetOutputBuffer() {
  DCHECK(output_buffer_);
  return output_buffer_;
}

bool PostProcessingPipelineImpl::SetOutputConfig(
    const AudioPostProcessor2::Config& output_config) {
  output_sample_rate_ = output_config.output_sample_rate;
  AudioPostProcessor2::Config processor_config = output_config;

  // Each Processor's output rate must be the following processor's input rate.
  for (int i = static_cast<int>(processors_.size()) - 1; i >= 0; --i) {
    if (!processors_[i].ptr->SetConfig(processor_config)) {
      return false;
    }
    int input_sample_rate = processors_[i].ptr->GetStatus().input_sample_rate;
    DCHECK_GT(input_sample_rate, 0)
        << processors_[i].name << " did not set its sample rate";
    processors_[i].input_frames_per_write =
        processor_config.output_frames_per_write * input_sample_rate /
        processor_config.output_sample_rate;
    processor_config.output_sample_rate = input_sample_rate;
    processor_config.output_frames_per_write =
        processors_[i].input_frames_per_write;
  }

  input_sample_rate_ = processor_config.output_sample_rate;
  ringing_time_in_frames_ = GetRingingTimeInFrames();
  silence_frames_processed_ = 0;

  if (input_sample_rate_ != output_sample_rate_) {
    size_t silence_size = num_output_channels_ *
                          processors_[0].input_frames_per_write *
                          output_sample_rate_ / input_sample_rate_;
    silence_buffer_.resize(silence_size);
  }

  delay_s_ = 0;
  for (auto& processor : processors_) {
    const auto& status = processor.ptr->GetStatus();
    delay_s_ += static_cast<double>(status.rendering_delay_frames) /
                status.input_sample_rate;
  }

  return true;
}

int PostProcessingPipelineImpl::GetInputSampleRate() const {
  return input_sample_rate_;
}

bool PostProcessingPipelineImpl::IsRinging() {
  return ringing_time_in_frames_ < 0 ||
         silence_frames_processed_ < ringing_time_in_frames_;
}

int PostProcessingPipelineImpl::GetRingingTimeInFrames() {
  int memory_frames = 0;
  for (auto& processor : processors_) {
    int ringing_time = processor.ptr->GetStatus().ringing_time_frames;
    if (ringing_time < 0) {
      return -1;
    }
    memory_frames += ringing_time;
  }
  return memory_frames;
}

void PostProcessingPipelineImpl::UpdateCastVolume(float multiplier,
                                                  float target) {
  DCHECK_GE(multiplier, 0.0);

  if (multiplier != current_multiplier_) {
    current_multiplier_ = multiplier;
    current_dbfs_ =
        (multiplier == 0.0f ? -200.0f : std::log10(multiplier) * 20);
    DCHECK(chromecast::media::VolumeControl::DbFSToVolume);
    cast_volume_ =
        chromecast::media::VolumeControl::DbFSToVolume(current_dbfs_);
  }

  if (target != target_multiplier_) {
    target_multiplier_ = target;
    target_dbfs_ = (target == 0.0f ? -200.0f : std::log10(target) * 20);
  }
}

// Send string |config| to postprocessor |name|.
void PostProcessingPipelineImpl::SetPostProcessorConfig(
    const std::string& name,
    const std::string& config) {
  DCHECK(!name.empty());
  std::vector<PostProcessorInfo>::iterator it =
      find_if(processors_.begin(), processors_.end(),
              [&name](PostProcessorInfo& p) { return p.name == name; });
  if (it != processors_.end()) {
    it->ptr->UpdateParameters(config);
    DVLOG(2) << "Config string: " << config
             << " was delivered to postprocessor " << name;
  }
}

// Set content type.
void PostProcessingPipelineImpl::SetContentType(AudioContentType content_type) {
  for (auto& processor : processors_) {
    processor.ptr->SetContentType(content_type);
  }
}

void PostProcessingPipelineImpl::UpdatePlayoutChannel(int channel) {
  for (auto& processor : processors_) {
    processor.ptr->SetPlayoutChannel(channel);
  }
}

double PostProcessingPipelineImpl::GetDelaySeconds() {
  return delay_s_;
}

}  // namespace media
}  // namespace chromecast
