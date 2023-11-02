// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/post_processor_registry.h"

#include <utility>

#include "base/no_destructor.h"
#include "chromecast/public/media/audio_post_processor2_shlib.h"

namespace chromecast {
namespace media {

// static
PostProcessorRegistry* PostProcessorRegistry::Get() {
  static base::NoDestructor<PostProcessorRegistry> g_registry;
  return g_registry.get();
}

void PostProcessorRegistry::Register(const std::string& library_name,
                                     CreateFunction create_function) {
  creators_.emplace(library_name, std::move(create_function));
}

std::unique_ptr<AudioPostProcessor2> PostProcessorRegistry::Create(
    const std::string& library_name,
    const std::string& config,
    int channels) {
  auto it = creators_.find(library_name);
  if (it == creators_.end()) {
    return nullptr;
  }
  return it->second.Run(config, channels);
}

}  // namespace media
}  // namespace chromecast
