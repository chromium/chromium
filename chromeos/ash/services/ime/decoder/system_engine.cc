// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/decoder/system_engine.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/services/ime/constants.h"

namespace ash {
namespace ime {

SystemEngine::SystemEngine(
    ImeCrosPlatform* platform,
    std::optional<ImeSharedLibraryWrapper::EntryPoints> entry_points) {
  if (!entry_points) {
    LOG(WARNING) << "SystemEngine INIT INCOMPLETE.";
    return;
  }

  decoder_entry_points_ = *entry_points;
  decoder_entry_points_->init_mojo_mode(platform);
}

SystemEngine::~SystemEngine() {
  if (!decoder_entry_points_) {
    return;
  }

  decoder_entry_points_->close_mojo_mode();
}

bool SystemEngine::BindConnectionFactory(
    mojo::PendingReceiver<mojom::ConnectionFactory> receiver) {
  if (!decoder_entry_points_) {
    return false;
  }
  auto receiver_pipe_handle = receiver.PassPipe().release().value();
  return decoder_entry_points_->mojo_mode_initialize_connection_factory(
      receiver_pipe_handle);
}

bool SystemEngine::IsConnected() {
  return decoder_entry_points_ &&
         decoder_entry_points_->mojo_mode_is_input_method_connected();
}

}  // namespace ime
}  // namespace ash
