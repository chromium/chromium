// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/receiver/receiver_creation.h"

#include "base/threading/sequence_bound.h"
#include "chromecast/media/audio/audio_io_thread.h"
#include "chromecast/media/audio/mixer_service/constants.h"
#include "chromecast/media/audio/mixer_service/receiver/receiver_cma.h"

namespace chromecast {
namespace media {
namespace mixer_service {

namespace {

class CmaReceiverInstance : public ReceiverInstance {
 public:
  explicit CmaReceiverInstance(MediaPipelineBackendManager* backend_manager)
      : receiver_(AudioIoThread::Get()->task_runner(), backend_manager) {}

  ~CmaReceiverInstance() override = default;

 private:
  base::SequenceBound<ReceiverCma> receiver_;
};

}  // namespace

std::unique_ptr<ReceiverInstance> CreateCmaReceiverIfNeeded(
    MediaPipelineBackendManager* backend_manager) {
  if (HaveFullMixer()) {
    return nullptr;
  }
  return std::make_unique<CmaReceiverInstance>(backend_manager);
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
