// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/decoder/decoder_engine.h"

#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/ime/constants.h"
#include "chromeos/services/ime/decoder/proto_conversion.h"
#include "chromeos/services/ime/ime_decoder.h"
#include "chromeos/services/ime/public/cpp/buildflags.h"
#include "chromeos/services/ime/public/proto/messages.pb.h"

namespace chromeos {
namespace ime {

namespace {

ImeEngineMainEntry* g_fake_main_entry_for_testing = nullptr;

using ReplyCallback =
    base::RepeatingCallback<void(const std::vector<uint8_t>&)>;

// A client delegate passed to the shared library in order for the
// shared library to send replies back to the engine.
class ClientDelegate : public ImeClientDelegate {
 public:
  // All replies from the shared library will be sent to both |remote| and
  // |callback|.
  ClientDelegate(const std::string& ime_spec,
                 mojo::PendingRemote<mojom::InputChannel> remote,
                 ReplyCallback callback)
      : ime_spec_(ime_spec),
        client_remote_(std::move(remote)),
        callback_(callback) {
    client_remote_.set_disconnect_handler(base::BindOnce(
        &ClientDelegate::OnDisconnected, base::Unretained(this)));
  }

  ~ClientDelegate() override {}

  const char* ImeSpec() override { return ime_spec_.c_str(); }

  void Process(const uint8_t* data, size_t size) override {
    if (client_remote_ && client_remote_.is_bound()) {
      std::vector<uint8_t> msg(data, data + size);
      client_remote_->ProcessMessage(msg, base::DoNothing());
      callback_.Run(msg);
    }
  }

  void Destroy() override {}

 private:
  void OnDisconnected() {
    client_remote_.reset();
    LOG(ERROR) << "Client remote is disconnected." << ime_spec_;
  }

  // The ime specification which is unique in the scope of engine.
  std::string ime_spec_;

  // The InputChannel remote used to talk to the client.
  mojo::Remote<mojom::InputChannel> client_remote_;

  ReplyCallback callback_;
};

std::vector<uint8_t> WrapAndSerializeMessage(PublicMessage message) {
  Wrapper wrapper;
  *wrapper.mutable_public_message() = std::move(message);
  std::vector<uint8_t> output(wrapper.ByteSizeLong());
  wrapper.SerializeToArray(output.data(), output.size());
  return output;
}

}  // namespace

void FakeEngineMainEntryForTesting(ImeEngineMainEntry* main_entry) {
  g_fake_main_entry_for_testing = main_entry;
}

DecoderEngine::DecoderEngine(ImeCrosPlatform* platform) : platform_(platform) {
  if (g_fake_main_entry_for_testing) {
    engine_main_entry_ = g_fake_main_entry_for_testing;
  } else {
    if (!TryLoadDecoder()) {
      LOG(WARNING) << "DecoderEngine INIT INCOMPLETED.";
    }
  }
}

DecoderEngine::~DecoderEngine() {}

bool DecoderEngine::TryLoadDecoder() {
  if (engine_main_entry_)
    return true;

  auto* decoder = ImeDecoder::GetInstance();
  if (decoder->GetStatus() == ImeDecoder::Status::kSuccess) {
    engine_main_entry_ = decoder->CreateMainEntry(platform_);
    return true;
  }
  return false;
}

bool DecoderEngine::BindRequest(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputChannel> receiver,
    mojo::PendingRemote<mojom::InputChannel> remote,
    const std::vector<uint8_t>& extra) {
  if (IsImeSupportedByDecoder(ime_spec)) {
    // Activates an IME engine via the shared library. Passing a
    // |ClientDelegate| for engine instance created by the shared library to
    // make safe calls on the client.
    if (engine_main_entry_->ActivateIme(
            ime_spec.c_str(),
            new ClientDelegate(ime_spec, std::move(remote),
                               base::BindRepeating(&DecoderEngine::OnReply,
                                                   base::Unretained(this))))) {
      decoder_channel_receivers_.Add(this, std::move(receiver));
      // TODO(https://crbug.com/837156): Registry connection error handler.
      return true;
    }
    return false;
  }

  // Otherwise, try the rule-based engine.
  return InputEngine::BindRequest(ime_spec, std::move(receiver),
                                  std::move(remote), extra);
}

bool DecoderEngine::IsImeSupportedByDecoder(const std::string& ime_spec) {
  return engine_main_entry_ &&
         engine_main_entry_->IsImeSupported(ime_spec.c_str());
}

void DecoderEngine::OnFocus() {
  const uint64_t seq_id = current_seq_id_;
  ++current_seq_id_;

  ProcessMessage(WrapAndSerializeMessage(OnFocusToProto(seq_id)),
                 base::DoNothing());
}

void DecoderEngine::OnBlur() {
  const uint64_t seq_id = current_seq_id_;
  ++current_seq_id_;

  ProcessMessage(WrapAndSerializeMessage(OnBlurToProto(seq_id)),
                 base::DoNothing());
}

void DecoderEngine::OnKeyEvent(mojom::PhysicalKeyEventPtr event,
                               OnKeyEventCallback callback) {
  const uint64_t seq_id = current_seq_id_;
  ++current_seq_id_;

  pending_key_event_callbacks_.emplace(seq_id, std::move(callback));
  ProcessMessage(
      WrapAndSerializeMessage(OnKeyEventToProto(seq_id, std::move(event))),
      base::DoNothing());
}

void DecoderEngine::OnSurroundingTextChanged(
    const std::string& text,
    uint32_t offset,
    mojom::SelectionRangePtr selection_range) {
  const uint64_t seq_id = current_seq_id_;
  ++current_seq_id_;

  ProcessMessage(WrapAndSerializeMessage(OnSurroundingTextChangedToProto(
                     seq_id, text, offset, std::move(selection_range))),
                 base::DoNothing());
}

void DecoderEngine::ProcessMessage(const std::vector<uint8_t>& message,
                                   ProcessMessageCallback callback) {
  // TODO(https://crbug.com/837156): Set a default protobuf message.
  std::vector<uint8_t> result;

  // Handle message via corresponding functions of loaded decoder.
  if (engine_main_entry_)
    engine_main_entry_->Process(message.data(), message.size());

  std::move(callback).Run(result);
}

void DecoderEngine::OnReply(const std::vector<uint8_t>& message) {
  if (!base::FeatureList::IsEnabled(
          chromeos::features::kSystemLatinPhysicalTyping)) {
    return;
  }

  ime::Wrapper wrapper;
  if (!wrapper.ParseFromArray(message.data(), message.size()) ||
      !wrapper.has_public_message()) {
    return;
  }

  const ime::PublicMessage& reply = wrapper.public_message();
  switch (reply.param_case()) {
    case ime::PublicMessage::kOnKeyEventReply: {
      const auto it = pending_key_event_callbacks_.find(reply.seq_id());
      CHECK(it != pending_key_event_callbacks_.end());
      auto callback = std::move(it->second);
      std::move(callback).Run(reply.on_key_event_reply().consumed());
      pending_key_event_callbacks_.erase(it);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace ime
}  // namespace chromeos
