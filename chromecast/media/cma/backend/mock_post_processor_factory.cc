// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mock_post_processor_factory.h"

#include "base/logging.h"
#include "base/values.h"

namespace chromecast {
namespace media {

using testing::_;
using testing::NiceMock;

MockPostProcessor::MockPostProcessor(
    MockPostProcessorFactory* factory,
    const std::string& name,
    const base::ListValue* filter_description_list,
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
  for (size_t i = 0; i < filter_description_list->GetSize(); ++i) {
    const base::DictionaryValue* description_dict;
    CHECK(filter_description_list->GetDictionary(i, &description_dict));
    std::string solib;
    CHECK(description_dict->GetString("processor", &solib));
    // This will initially be called with the actual pipeline on creation.
    // Ignore and wait for the call to ResetPostProcessorsForTest.
    const std::string kDelayModuleSolib = "delay.so";
    if (solib == kDelayModuleSolib) {
      const base::DictionaryValue* processor_config_dict;
      CHECK(description_dict->GetDictionary("config", &processor_config_dict));
      int module_delay;
      CHECK(processor_config_dict->GetInteger("delay", &module_delay));
      rendering_delay_ += module_delay;
      processor_config_dict->GetBoolean("ringing", &ringing_);
      processor_config_dict->GetInteger("output_channels",
                                        &num_output_channels_);
    }
  }
}

MockPostProcessor::~MockPostProcessor() {
  factory_->instances.erase(name_);
}

std::unique_ptr<PostProcessingPipeline>
MockPostProcessorFactory::CreatePipeline(
    const std::string& name,
    const base::ListValue* filter_description_list,
    int channels) {
  return std::make_unique<testing::NiceMock<MockPostProcessor>>(
      this, name, filter_description_list, channels);
}

MockPostProcessorFactory::MockPostProcessorFactory() = default;
MockPostProcessorFactory::~MockPostProcessorFactory() = default;

}  // namespace media
}  // namespace chromecast
