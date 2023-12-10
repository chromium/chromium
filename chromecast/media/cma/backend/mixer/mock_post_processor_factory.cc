// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mock_post_processor_factory.h"

#include "base/check.h"
#include "base/values.h"

namespace chromecast {
namespace media {

using testing::_;
using testing::NiceMock;

MockPostProcessor::MockPostProcessor(MockPostProcessorFactory* factory,
                                     const std::string& name,
                                     const base::Value* filter_description_list,
                                     int channels)
    : factory_(factory), name_(name), num_output_channels_(channels) {
  DCHECK(factory_);
  CHECK(factory_->instances.insert({name_, this}).second);

  ON_CALL(*this, ProcessFrames(_, _, _, _, _))
      .WillByDefault(
          testing::Invoke(this, &MockPostProcessor::DoProcessFrames));

  if (!filter_description_list) {
    // This happens for PostProcessingPipeline with no post-processors.
    return;
  }

  // Parse |filter_description_list| for parameters.
  for (const base::Value& elem : filter_description_list->GetList()) {
    CHECK(elem.is_dict());
    const std::string* solib = elem.GetDict().FindString("processor");
    CHECK(solib);

    if (*solib == "delay.so") {
      const base::Value::Dict* processor_config_dict =
          elem.GetDict().FindDict("config");
      CHECK(processor_config_dict);
      std::optional<int> delay = processor_config_dict->FindInt("delay");
      CHECK(delay.has_value());
      rendering_delay_frames_ += *delay;
      std::optional<bool> ringing = processor_config_dict->FindBool("ringing");
      if (ringing.has_value()) {
        ringing_ = *ringing;
      }

      std::optional<int> output_ch =
          processor_config_dict->FindInt("output_channels");
      if (output_ch.has_value()) {
        num_output_channels_ = *output_ch;
      }
    }
  }
}

MockPostProcessor::~MockPostProcessor() {
  factory_->instances.erase(name_);
}

std::unique_ptr<PostProcessingPipeline>
MockPostProcessorFactory::CreatePipeline(
    const std::string& name,
    const base::Value* filter_description_list,
    int channels) {
  return std::make_unique<testing::NiceMock<MockPostProcessor>>(
      this, name, filter_description_list, channels);
}

MockPostProcessorFactory::MockPostProcessorFactory() = default;
MockPostProcessorFactory::~MockPostProcessorFactory() = default;

}  // namespace media
}  // namespace chromecast
