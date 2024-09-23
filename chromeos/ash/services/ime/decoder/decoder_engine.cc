// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/services/ime/decoder/decoder_engine.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "chromeos/ash/services/ime/constants.h"

namespace ash {
namespace ime {

namespace {

// A client delegate passed to the shared library in order for the
// shared library to send replies back to the engine.
class ClientDelegate : public ImeClientDelegate {
 public:
  // All replies from the shared library will be sent to both |remote| and
  // |callback|.
  ClientDelegate(const std::string& ime_spec,
                 mojo::PendingRemote<mojom::InputChannel> remote)
      : ime_spec_(ime_spec), client_remote_(std::move(remote)) {
    client_remote_.set_disconnect_handler(base::BindOnce(
        &ClientDelegate::OnDisconnected, base::Unretained(this)));
  }

  ~ClientDelegate() override {}

  void Unused1() override { NOTIMPLEMENTED(); }

  void Process(const uint8_t* data, size_t size) override {
    if (client_remote_ && client_remote_.is_bound()) {
      std::vector<uint8_t> msg(data, data + size);
      client_remote_->ProcessMessage(msg, base::DoNothing());
    }
  }

  void Destroy() override { delete this; }

 private:
  void OnDisconnected() {
    client_remote_.reset();
    LOG(ERROR) << "Client remote is disconnected." << ime_spec_;
  }

  // The ime specification which is unique in the scope of engine.
  std::string ime_spec_;

  // The InputChannel remote used to talk to the client.
  mojo::Remote<mojom::InputChannel> client_remote_;
};

}  // namespace

DecoderEngine::DecoderEngine(
    ImeCrosPlatform* platform,
    std::optional<ImeSharedLibraryWrapper::EntryPoints> entry_points) {
  if (!entry_points) {
    LOG(WARNING) << "DecoderEngine INIT INCOMPLETE.";
    return;
  }

  decoder_entry_points_ = *entry_points;
  decoder_entry_points_->init_proto_mode(platform);
}

DecoderEngine::~DecoderEngine() {
  if (!decoder_entry_points_) {
    return;
  }

  decoder_entry_points_->close_proto_mode();
}

bool DecoderEngine::BindRequest(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputChannel> receiver,
    mojo::PendingRemote<mojom::InputChannel> remote,
    const std::vector<uint8_t>& extra) {
  // Activates an IME engine via the shared lib. Passing a |ClientDelegate| for
  // engine instance created by the shared lib to make safe calls on the client.
  if (decoder_entry_points_ &&
      decoder_entry_points_->proto_mode_supports(ime_spec.c_str()) &&
      decoder_entry_points_->proto_mode_activate_ime(
          ime_spec.c_str(), new ClientDelegate(ime_spec, std::move(remote)))) {
    decoder_channel_receivers_.Add(this, std::move(receiver));
    // TODO(https://crbug.com/837156): Registry connection error handler.
    return true;
  }
  return false;
}

void DecoderEngine::ProcessMessage(const std::vector<uint8_t>& message,
                                   ProcessMessageCallback callback) {
  // TODO(https://crbug.com/837156): Set a default protobuf message.
  std::vector<uint8_t> result;

  // Handle message via corresponding functions of loaded decoder.
  if (decoder_entry_points_) {
    decoder_entry_points_->proto_mode_process(message.data(), message.size());
  }

  std::move(callback).Run(result);
}

}  // namespace ime
}  // namespace ash
