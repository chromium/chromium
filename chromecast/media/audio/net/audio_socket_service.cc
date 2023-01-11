// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/net/audio_socket_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/net_errors.h"
#include "net/socket/server_socket.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {

AudioSocketService::~AudioSocketService() = default;

void AudioSocketService::Accept() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!listen_socket_) {
    return;
  }

  for (int i = 0; i < max_accept_loop_; ++i) {
    int result = AcceptOne();
    // If the result is ERR_IO_PENDING, OnAccept() will eventually be
    // called; it will resume the accept loop.
    if (result == net::ERR_IO_PENDING || !HandleAcceptResult(result)) {
      return;
    }
  }
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&AudioSocketService::Accept,
                                                   weak_factory_.GetWeakPtr()));
}

void AudioSocketService::OnAsyncAcceptComplete(int result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (HandleAcceptResult(result)) {
    Accept();
  }
}

bool AudioSocketService::HandleAcceptResult(int result) {
  if (result != net::OK) {
    LOG(ERROR) << "Accept failed: " << net::ErrorToString(result);
    return false;
  }
  OnAcceptSuccess();
  return true;
}

}  // namespace media
}  // namespace chromecast
