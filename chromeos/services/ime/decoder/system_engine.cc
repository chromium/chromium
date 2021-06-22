// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/decoder/system_engine.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chromeos/services/ime/constants.h"
#include "chromeos/services/ime/decoder/proto_conversion.h"
#include "chromeos/services/ime/public/cpp/buildflags.h"
#include "chromeos/services/ime/public/proto/messages.pb.h"

namespace chromeos {
namespace ime {

namespace {

using ReplyCallback =
    base::RepeatingCallback<void(const std::vector<uint8_t>&,
                                 mojo::Remote<mojom::InputChannel>& delegate)>;

// A client delegate passed to the shared library in order for the
// shared library to send replies back to the engine.
class ClientDelegate : public ImeClientDelegate {
 public:
  // All replies from the shared library will be sent to both |delegate| and
  // |callback|.
  ClientDelegate(const std::string& ime_spec,
                 mojo::PendingRemote<mojom::InputChannel> delegate,
                 ReplyCallback callback)
      : ime_spec_(ime_spec),
        delegate_(std::move(delegate)),
        callback_(callback) {
    delegate_.set_disconnect_handler(base::BindOnce(
        &ClientDelegate::OnDisconnected, base::Unretained(this)));
  }

  ~ClientDelegate() override {}

  const char* ImeSpec() override { return ime_spec_.c_str(); }

  void Process(const uint8_t* data, size_t size) override {
    if (delegate_.is_bound()) {
      std::vector<uint8_t> msg(data, data + size);
      delegate_->ProcessMessage(msg, base::DoNothing());
      callback_.Run(msg, delegate_);
    }
  }

  void Destroy() override {}

 private:
  void OnDisconnected() {
    delegate_.reset();
    LOG(ERROR) << "Client remote is disconnected." << ime_spec_;
  }

  // The ime specification which is unique in the scope of engine.
  std::string ime_spec_;

  // The InputChannel used to talk to Chrome.
  mojo::Remote<mojom::InputChannel> delegate_;

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

SystemEngine::SystemEngine(ImeCrosPlatform* platform) : platform_(platform) {
  if (!TryLoadDecoder()) {
    LOG(WARNING) << "DecoderEngine INIT INCOMPLETED.";
  }
}

SystemEngine::~SystemEngine() {}

bool SystemEngine::TryLoadDecoder() {
  auto* decoder = ImeDecoder::GetInstance();
  if (decoder->GetStatus() == ImeDecoder::Status::kSuccess &&
      decoder->GetEntryPoints().is_ready) {
    decoder_entry_points_ = decoder->GetEntryPoints();
    decoder_entry_points_->init_once(platform_);
    return true;
  }
  return false;
}

bool SystemEngine::BindRequest(
    const std::string& ime_spec,
    mojo::PendingReceiver<mojom::InputMethod> receiver,
    mojo::PendingRemote<mojom::InputChannel> delegate,
    base::OnceCallback<void()> disconnect_callback) {
  if (!IsImeSupportedByDecoder(ime_spec)) {
    return false;
  }

  // Activates an IME engine via the shared library. Passing a
  // |ClientDelegate| for engine instance created by the shared library to
  // make safe calls on the client.
  if (!decoder_entry_points_->activate_ime(
          ime_spec.c_str(),
          new ClientDelegate(ime_spec, std::move(delegate),
                             base::BindRepeating(&SystemEngine::OnReply,
                                                 base::Unretained(this))))) {
    return false;
  }

  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(std::move(disconnect_callback));

  OnInputMethodChanged(ime_spec);
  return true;
}

bool SystemEngine::IsImeSupportedByDecoder(const std::string& ime_spec) {
  return decoder_entry_points_ &&
         decoder_entry_points_->supports(ime_spec.c_str());
}

void SystemEngine::OnFocus(mojom::InputFieldInfoPtr input_field_info) {
  const uint64_t seq_id = current_seq_id_;
  ++current_seq_id_;

  ProcessMessage(WrapAndSerializeMessage(
      OnFocusToProto(seq_id, std::move(input_field_info))));
}

void SystemEngine::OnBlur() {
  const uint64_t seq_id = current_seq_id_;
  ++current_seq_id_;

  ProcessMessage(WrapAndSerializeMessage(OnBlurToProto(seq_id)));
}

void SystemEngine::OnKeyEvent(mojom::PhysicalKeyEventPtr event,
                              OnKeyEventCallback callback) {
  const uint64_t seq_id = current_seq_id_;
  ++current_seq_id_;

  pending_key_event_callbacks_.emplace(seq_id, std::move(callback));
  ProcessMessage(
      WrapAndSerializeMessage(OnKeyEventToProto(seq_id, std::move(event))));
}

void SystemEngine::OnSurroundingTextChanged(
    const std::string& text,
    uint32_t offset,
    mojom::SelectionRangePtr selection_range) {
  const uint64_t seq_id = current_seq_id_;
  ++current_seq_id_;

  ProcessMessage(WrapAndSerializeMessage(OnSurroundingTextChangedToProto(
      seq_id, text, offset, std::move(selection_range))));
}

void SystemEngine::OnCompositionCanceledBySystem() {
  const uint64_t seq_id = current_seq_id_;
  ++current_seq_id_;

  ProcessMessage(WrapAndSerializeMessage(OnCompositionCanceledToProto(seq_id)));
}

void SystemEngine::OnSuggestionsReturned(
    mojom::SuggestionsResponsePtr response) {
  const uint64_t seq_id = current_seq_id_;
  ++current_seq_id_;

  ProcessMessage(WrapAndSerializeMessage(
      SuggestionsResponseToProto(seq_id, std::move(response))));
}

void SystemEngine::ProcessMessage(const std::vector<uint8_t>& message) {
  // Handle message via corresponding functions of loaded decoder.
  if (decoder_entry_points_)
    decoder_entry_points_->process(message.data(), message.size());
}

void SystemEngine::OnInputMethodChanged(const std::string& engine_id) {
  const uint64_t seq_id = current_seq_id_;
  ++current_seq_id_;

  ProcessMessage(
      WrapAndSerializeMessage(OnInputMethodChangedToProto(seq_id, engine_id)));
}

void SystemEngine::OnReply(const std::vector<uint8_t>& message,
                           mojo::Remote<mojom::InputChannel>& delegate) {
  ime::Wrapper wrapper;
  if (!wrapper.ParseFromArray(message.data(), message.size()) ||
      !wrapper.has_public_message()) {
    return;
  }

  const ime::PublicMessage& reply = wrapper.public_message();
  // TODO(crbug/1146266): Add case to handle request for suggestions.
  switch (reply.param_case()) {
    case ime::PublicMessage::kOnKeyEventReply: {
      const auto it = pending_key_event_callbacks_.find(reply.seq_id());
      CHECK(it != pending_key_event_callbacks_.end());
      auto callback = std::move(it->second);
      std::move(callback).Run(reply.on_key_event_reply().consumed());
      pending_key_event_callbacks_.erase(it);
      break;
    }
    case ime::PublicMessage::kSetComposition: {
      delegate->SetComposition(reply.set_composition().text());
      break;
    }
    case ime::PublicMessage::kSetCompositionRange: {
      delegate->SetCompositionRange(
          reply.set_composition_range().start_byte_index(),
          reply.set_composition_range().end_byte_index());
      break;
    }
    case ime::PublicMessage::kFinishComposition: {
      delegate->FinishComposition();
      break;
    }
    case ime::PublicMessage::kDeleteSurroundingText: {
      delegate->DeleteSurroundingText(
          reply.delete_surrounding_text().num_bytes_before_cursor(),
          reply.delete_surrounding_text().num_bytes_after_cursor());
      break;
    }
    case ime::PublicMessage::kCommitText: {
      delegate->CommitText(
          reply.commit_text().text(),
          reply.commit_text().cursor_behavior() ==
                  ime::CommitTextCursorBehavior::
                      COMMIT_TEXT_CURSOR_BEHAVIOR_MOVE_CURSOR_BEFORE_TEXT
              ? mojom::CommitTextCursorBehavior::kMoveCursorBeforeText
              : mojom::CommitTextCursorBehavior::kMoveCursorAfterText);
      break;
    }
    case ime::PublicMessage::kHandleAutocorrect: {
      delegate->HandleAutocorrect(ProtoToAutocorrectSpan(
          reply.handle_autocorrect().autocorrect_span()));
      break;
    }
    case ime::PublicMessage::kSuggestionsRequest: {
      delegate->RequestSuggestions(
          ProtoToSuggestionsRequest(reply.suggestions_request()),
          base::BindOnce(&SystemEngine::OnSuggestionsReturned,
                         base::Unretained(this)));
      break;
    }
    case ime::PublicMessage::kDisplaySuggestions: {
      delegate->DisplaySuggestions(
          ProtoToTextSuggestions(reply.display_suggestions()));
      break;
    }
    case ime::PublicMessage::kRecordUkm: {
      auto ukm = ProtoToUkmEntry(reply.record_ukm());
      if (ukm)
        delegate->RecordUkm(std::move(ukm));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace ime
}  // namespace chromeos
