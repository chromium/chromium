// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/mixer_control.h"

#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chromecast/media/audio/audio_io_thread.h"
#include "chromecast/media/audio/mixer_service/constants.h"
#include "chromecast/media/audio/mixer_service/control_connection.h"

namespace chromecast {
namespace media {
namespace mixer_service {

// static
MixerControl* MixerControl::Get() {
  if (HaveFullMixer()) {
    static base::NoDestructor<MixerControl> instance;
    return instance.get();
  }
  return nullptr;
}

MixerControl::MixerControl() : control_(AudioIoThread::Get()->task_runner()) {
  DCHECK(HaveFullMixer());
  control_.Post(FROM_HERE, &ControlConnection::Connect,
                ControlConnection::ConnectedCallback());
}

MixerControl::~MixerControl() = default;

void MixerControl::ConfigurePostprocessor(std::string postprocessor_name,
                                          std::string config) {
  control_.Post(FROM_HERE, &ControlConnection::ConfigurePostprocessor,
                std::move(postprocessor_name), std::move(config));
}

void MixerControl::ReloadPostprocessors() {
  control_.Post(FROM_HERE, &ControlConnection::ReloadPostprocessors);
}

void MixerControl::SetNumOutputChannels(int num_channels) {
  control_.Post(FROM_HERE, &ControlConnection::SetNumOutputChannels,
                num_channels);
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
