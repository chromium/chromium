// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/mixer/loopback_test_support.h"

#include <utility>

#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/media/cma/backend/mixer/loopback_handler.h"
#include "chromecast/media/cma/backend/mixer/mixer_loopback_connection.h"

namespace chromecast {
namespace media {

namespace {

class FakeMixerDelegate : public mixer_service::MixerSocket::Delegate {
 public:
  ~FakeMixerDelegate() override = default;
};

}  // namespace

std::unique_ptr<mixer_service::MixerSocket> CreateLoopbackConnectionForTest(
    LoopbackHandler* loopback_handler) {
  auto receiver_socket = std::make_unique<mixer_service::MixerSocket>();
  auto caller_socket = std::make_unique<mixer_service::MixerSocket>();

  receiver_socket->SetLocalCounterpart(caller_socket->GetWeakPtr(),
                                       base::SequencedTaskRunnerHandle::Get());
  caller_socket->SetLocalCounterpart(receiver_socket->GetWeakPtr(),
                                     base::SequencedTaskRunnerHandle::Get());

  auto mixer_side =
      std::make_unique<MixerLoopbackConnection>(std::move(receiver_socket));
  loopback_handler->AddConnection(std::move(mixer_side));

  return caller_socket;
}

}  // namespace media
}  // namespace chromecast
