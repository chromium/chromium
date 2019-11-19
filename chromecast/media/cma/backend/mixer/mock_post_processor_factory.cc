// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/mock_post_processor_factory.h"

#include "base/logging.h"
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

  ON_CALL(*this, ProcessFrames(_, _, _, _))
      .WillByDefault(
          testing::Invoke(this, &MockPostProcessor::DoProcessFrames));

  if (!filter_description_list) {
    // This happens for PostProcessingPipeline with no post-processors.
    return;
  }

  // Parse |filter_description_list| for parameters.
  for (const base::Value& elem : filter_description_list->GetList()) {
    CHECK(elem.is_dict());
    const base::Value* processor_val =
        elem.FindKeyOfType("processor", base::Value::Type::STRING);
    CHECK(processor_val);
    std::string solib = processor_val->GetString();

    if (solib == "delay.so") {
      const base::Value* processor_config_dict =
          elem.FindKeyOfType("config", base::Value::Type::DICTIONARY);
      CHECK(processor_config_dict);
      const base::Value* delay_val = processor_config_dict->FindKeyOfType(
          "delay", base::Value::Type::INTEGER);
      CHECK(delay_val);
      rendering_delay_frames_ += delay_val->GetInt();
      const base::Value* ringing_val = processor_config_dict->FindKeyOfType(
          "ringing", base::Value::Type::BOOLEAN);
      if (ringing_val) {
        ringing_ = ringing_val->GetBool();
      }

      const base::Value* output_ch_val = processor_config_dict->FindKeyOfType(
          "output_channels", base::Value::Type::INTEGER);
      if (output_ch_val) {
        num_output_channels_ = output_ch_val->GetInt();
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
