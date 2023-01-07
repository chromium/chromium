// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/mixer_control.h"

#include <utility>

#include "base/check.h"
#include "base/location.h"
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
  control_.AsyncCall(&ControlConnection::Connect)
      .WithArgs(ControlConnection::ConnectedCallback());
}

MixerControl::~MixerControl() = default;

void MixerControl::ConfigurePostprocessor(std::string postprocessor_name,
                                          std::string config) {
  control_.AsyncCall(&ControlConnection::ConfigurePostprocessor)
      .WithArgs(std::move(postprocessor_name), std::move(config));
}

void MixerControl::ListPostprocessors(ListPostprocessorsCallback callback) {
  control_.AsyncCall(&ControlConnection::ListPostprocessors)
      .WithArgs(std::move(callback));
}
void MixerControl::ReloadPostprocessors() {
  control_.AsyncCall(&ControlConnection::ReloadPostprocessors);
}

void MixerControl::SetNumOutputChannels(int num_channels) {
  control_.AsyncCall(&ControlConnection::SetNumOutputChannels)
      .WithArgs(num_channels);
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
