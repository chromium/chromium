// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_FILE_PAYLOAD_LISTENER_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_FILE_PAYLOAD_LISTENER_H_

#include <vector>

#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

namespace secure_channel {

// Test FilePayloadListener implementation.
class FakeFilePayloadListener : public mojom::FilePayloadListener {
 public:
  FakeFilePayloadListener();
  FakeFilePayloadListener(const FakeFilePayloadListener&) = delete;
  FakeFilePayloadListener& operator=(const FakeFilePayloadListener&) = delete;
  ~FakeFilePayloadListener() override;

  mojo::PendingRemote<mojom::FilePayloadListener> GenerateRemote();

  void OnDisconnect();

  mojo::Receiver<mojom::FilePayloadListener>& receiver() { return receiver_; }

  const std::vector<mojom::FileTransferUpdatePtr>& received_updates() const {
    return received_updates_;
  }

  bool is_connected() const { return is_connected_; }

 private:
  // mojom::MessageReceiver:
  void OnFileTransferUpdate(mojom::FileTransferUpdatePtr update) override;

  mojo::Receiver<mojom::FilePayloadListener> receiver_{this};

  std::vector<mojom::FileTransferUpdatePtr> received_updates_;
  bool is_connected_ = false;
};

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_FAKE_FILE_PAYLOAD_LISTENER_H_
