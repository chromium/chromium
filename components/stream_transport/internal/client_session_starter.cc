// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stream_transport/internal/client_session_starter.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/stream_transport/internal/proto/transport_messages.pb.h"
#include "components/stream_transport/internal/transport_channel_impl.h"
#include "components/stream_transport/internal/transport_session_impl.h"
#include "components/stream_transport/internal/transport_session_registry_impl.h"
#include "components/stream_transport/public/common.h"
#include "components/stream_transport/public/transport_service.h"
#include "components/stream_transport/public/transport_session.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace stream_transport {

PendingTransportSessionImpl::BufferedMessage::BufferedMessage(
    PayloadType payload_type,
    std::unique_ptr<google::protobuf::MessageLite> message)
    : payload_type(payload_type), message(std::move(message)) {}

PendingTransportSessionImpl::BufferedMessage::BufferedMessage(
    BufferedMessage&&) noexcept = default;

PendingTransportSessionImpl::BufferedMessage&
PendingTransportSessionImpl::BufferedMessage::operator=(
    BufferedMessage&&) noexcept = default;

PendingTransportSessionImpl::BufferedMessage::~BufferedMessage() = default;

PendingTransportSessionImpl::PendingTransportSessionImpl(
    std::string_view session_id,
    base::OnceClosure on_close)
    : session_id_(session_id), on_close_(std::move(on_close)) {}

PendingTransportSessionImpl::~PendingTransportSessionImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::string_view PendingTransportSessionImpl::GetSessionId() const {
  return session_id_;
}

base::expected<void, SendUpstreamMessageError>
PendingTransportSessionImpl::SendUpstreamMessage(
    PayloadType payload_type,
    const google::protobuf::MessageLite& message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(payload_type, PayloadType::kUnspecified);
  if (closed_) {
    return base::unexpected(SendUpstreamMessageError::kChannelDisconnected);
  }
  if (buffered_messages_.size() >= kMaxBufferedOutboundMessages) {
    return base::unexpected(SendUpstreamMessageError::kBufferFull);
  }
  std::unique_ptr<google::protobuf::MessageLite> copy(
      message.New(/*arena=*/nullptr));
  copy->CheckTypeAndMergeFrom(message);
  buffered_messages_.emplace_back(payload_type, std::move(copy));
  return {};
}

void PendingTransportSessionImpl::OnMessage(
    PayloadType payload_type,
    std::string_view serialized_payload) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PendingTransportSessionImpl::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  closed_ = true;
  buffered_messages_.clear();
  if (on_close_) {
    std::move(on_close_).Run();
  }
}

void PendingTransportSessionImpl::MarkClosed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  closed_ = true;
  buffered_messages_.clear();
}

std::vector<PendingTransportSessionImpl::BufferedMessage>
PendingTransportSessionImpl::TakeBufferedMessages() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  closed_ = true;
  return std::exchange(buffered_messages_, {});
}

ClientSessionStarter::ClientSessionStarter(
    TransportChannelImpl* channel,
    signin::IdentityManager* identity_manager)
    : channel_(channel ? channel->GetImplWeakPtr() : nullptr),
      identity_manager_(CHECK_DEREF(identity_manager)) {
  CHECK(channel_);
}

ClientSessionStarter::~ClientSessionStarter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

TransportSession* ClientSessionStarter::StartClientSession(
    std::string_view session_id,
    PayloadType payload_type,
    const google::protobuf::MessageLite& message,
    TransportService::StartClientSessionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* registry = channel_ ? channel_->session_registry() : nullptr;
  if (!registry) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(StartClientSessionError::kChannelUnavailable)));
    return nullptr;
  }

  const CoreAccountId account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (account_id.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(StartClientSessionError::kChannelUnavailable)));
    return nullptr;
  }

  size_t active_pending_count = 0;
  for (const auto& [_, pending] : pending_starts_) {
    if (pending.aborted) {
      continue;
    }
    ++active_pending_count;
    if (pending.session_id == session_id) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(callback),
              base::unexpected(StartClientSessionError::kSessionIdConflict)));
      return nullptr;
    }
  }

  if (registry->GetSessionImpl(session_id)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(StartClientSessionError::kSessionIdConflict)));
    return nullptr;
  }

  if (!registry->CanCreateSession(session_id) ||
      registry->GetAllSessionImpls().size() + active_pending_count >=
          registry->max_concurrent_sessions()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            base::unexpected(StartClientSessionError::kSessionLimitReached)));
    return nullptr;
  }

  const uint64_t start_id = next_start_id_++;
  auto pending_session = std::make_unique<PendingTransportSessionImpl>(
      session_id, base::BindOnce(&ClientSessionStarter::CancelPendingStart,
                                 weak_ptr_factory_.GetWeakPtr(), start_id));
  TransportSession* session_ptr = pending_session.get();
  pending_starts_.emplace(
      start_id, PendingStart{std::string(session_id), account_id,
                             std::move(pending_session), std::move(callback)});

  channel_->SendInitialUpstreamMessage(
      session_id, payload_type, message,
      base::BindOnce(&ClientSessionStarter::OnInitialMessageSent,
                     weak_ptr_factory_.GetWeakPtr(), start_id));
  return session_ptr;
}

void ClientSessionStarter::CancelPendingStart(uint64_t start_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (auto it = pending_starts_.find(start_id); it != pending_starts_.end()) {
    it->second.cancelled = true;
    it->second.callback.Reset();
  }
}

void ClientSessionStarter::PostFailedStart(uint64_t start_id,
                                           StartClientSessionError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ClientSessionStarter::OnPostFailedStart,
                     weak_ptr_factory_.GetWeakPtr(), start_id, error));
}

void ClientSessionStarter::OnPostFailedStart(uint64_t start_id,
                                             StartClientSessionError error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = pending_starts_.find(start_id);
  if (it == pending_starts_.end()) {
    return;
  }
  PendingStart entry = std::move(it->second);
  pending_starts_.erase(it);
  entry.pending_session->MarkClosed();
  if (!entry.cancelled && entry.callback) {
    std::move(entry.callback).Run(base::unexpected(error));
  }
}

void ClientSessionStarter::OnInitialMessageSent(uint64_t start_id,
                                                int http_status_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = pending_starts_.find(start_id);
  if (it == pending_starts_.end()) {
    return;
  }
  PendingStart entry = std::move(it->second);
  pending_starts_.erase(it);
  // Retire the pending handle immediately so re-entrant sends during session
  // registration, buffer flushing, or callback execution fail with
  // `kChannelDisconnected` before `entry` is destroyed at scope exit.
  std::vector<PendingTransportSessionImpl::BufferedMessage> buffered_messages =
      entry.pending_session->TakeBufferedMessages();

  const bool is_2xx = http_status_code >= 200 && http_status_code < 300;
  const bool same_account =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin) ==
      entry.account_id;
  auto* registry = channel_ ? channel_->session_registry() : nullptr;

  if (entry.cancelled) {
    if (is_2xx && same_account) {
      SendUnregisteredCloseSession(entry.session_id);
    }
    return;
  }

  if (!registry) {
    std::move(entry.callback)
        .Run(base::unexpected(StartClientSessionError::kChannelUnavailable));
    return;
  }

  if (!same_account) {
    std::move(entry.callback)
        .Run(base::unexpected(StartClientSessionError::kChannelUnavailable));
    return;
  }

  if (is_2xx) {
    const base::WeakPtr<TransportChannelImpl> channel = channel_;
    TransportSessionImpl* session = registry->CreateSession(
        entry.session_id, InitIngressSource::kLocalChromeIngest);
    if (!session) {
      // Concurrent session limit reached while sequence 1 was in flight.
      if (channel) {
        SendUnregisteredCloseSession(entry.session_id);
      }
      std::move(entry.callback)
          .Run(base::unexpected(StartClientSessionError::kSessionLimitReached));
      return;
    }
    // Flush outbound messages buffered while pending in FIFO order before
    // invoking `callback`, so sequence numbers (2..N) stay ordered ahead of
    // any sends initiated from inside `callback`.
    for (auto& buffered : buffered_messages) {
      if (!channel ||
          channel->session_registry()->GetSessionImpl(entry.session_id) !=
              session ||
          !session->SendUpstreamMessage(buffered.payload_type, *buffered.message)
               .has_value()) {
        if (channel &&
            channel->session_registry()->GetSessionImpl(entry.session_id) ==
                session) {
          session->Close();
        }
        std::move(entry.callback)
            .Run(base::unexpected(StartClientSessionError::kChannelUnavailable));
        return;
      }
    }
    if (!channel ||
        channel->session_registry()->GetSessionImpl(entry.session_id) !=
            session) {
      std::move(entry.callback)
          .Run(base::unexpected(StartClientSessionError::kChannelUnavailable));
      return;
    }
    std::move(entry.callback).Run(base::ok(session));
    return;
  }

  std::move(entry.callback)
      .Run(base::unexpected(
          StartClientSessionErrorFromHttpStatus(http_status_code)));
}

void ClientSessionStarter::SendUnregisteredCloseSession(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* registry = channel_ ? channel_->session_registry() : nullptr;
  if (!channel_ || !registry || registry->GetSession(session_id)) {
    return;
  }
  browser_actuator::ControlCommand close_command;
  close_command.mutable_close_session();
  // Sequence 1 was consumed by the initial start-session upstream message.
  channel_->SendUnregisteredUpstreamMessage(
      session_id, /*client_sequence_number=*/2, PayloadType::kControl,
      close_command);
}

void ClientSessionStarter::AbortPendingStartsForAccountChange() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& [id, entry] : pending_starts_) {
    if (!entry.aborted) {
      entry.aborted = true;
      entry.pending_session->MarkClosed();
      PostFailedStart(id, StartClientSessionError::kChannelUnavailable);
    }
  }
}

void ClientSessionStarter::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::flat_map<uint64_t, PendingStart> pending = std::move(pending_starts_);
  pending_starts_.clear();
  for (auto& [id, entry] : pending) {
    entry.pending_session->MarkClosed();
  }
  for (auto& [id, entry] : pending) {
    if (!entry.cancelled && entry.callback) {
      std::move(entry.callback)
          .Run(base::unexpected(StartClientSessionError::kChannelUnavailable));
    }
  }
}

}  // namespace stream_transport
