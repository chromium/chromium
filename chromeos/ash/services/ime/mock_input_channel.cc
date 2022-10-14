// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/ime/mock_input_channel.h"

namespace ash {
namespace ime {

MockInputChannel::MockInputChannel() : receiver_(this) {}

MockInputChannel::~MockInputChannel() = default;

mojo::PendingRemote<mojom::InputChannel>
MockInputChannel::CreatePendingRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

bool MockInputChannel::IsBound() const {
  return receiver_.is_bound();
}

void MockInputChannel::FlushForTesting() {
  return receiver_.FlushForTesting();
}

void MockInputChannel::ProcessMessage(const std::vector<uint8_t>& message,
                                      ProcessMessageCallback callback) {
  std::move(callback).Run({});
}

}  // namespace ime
}  // namespace ash
