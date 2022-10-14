// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_MOCK_INPUT_CHANNEL_H_
#define CHROMEOS_ASH_SERVICES_IME_MOCK_INPUT_CHANNEL_H_

#include "chromeos/ash/services/ime/public/mojom/input_engine.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_host.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace ime {

// A mock receiver InputChannel.
class MockInputChannel : public mojom::InputChannel {
 public:
  MockInputChannel();
  ~MockInputChannel() override;
  MockInputChannel(const MockInputChannel&) = delete;
  MockInputChannel& operator=(const MockInputChannel&) = delete;

  mojo::PendingRemote<mojom::InputChannel> CreatePendingRemote();
  bool IsBound() const;
  void FlushForTesting();

  // mojom::InputChannel:
  void ProcessMessage(const std::vector<uint8_t>& message,
                      ProcessMessageCallback callback) override;

 private:
  mojo::Receiver<mojom::InputChannel> receiver_;
};

}  // namespace ime
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_IME_MOCK_INPUT_CHANNEL_H_
