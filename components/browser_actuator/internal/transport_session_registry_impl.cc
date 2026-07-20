// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_session_registry_impl.h"

#include <utility>

#include "base/check.h"
#include "components/browser_actuator/internal/transport_session_impl.h"

namespace browser_actuator {

TransportSessionRegistryImpl::TransportSessionRegistryImpl(
    base::WeakPtr<TransportChannel> channel)
    : channel_(std::move(channel)) {
  DCHECK(channel_);
}

TransportSessionRegistryImpl::~TransportSessionRegistryImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

TransportSession* TransportSessionRegistryImpl::GetSession(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetSessionImpl(session_id);
}

std::vector<TransportSessionImpl*>
TransportSessionRegistryImpl::GetAllSessionImpls() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<TransportSessionImpl*> active_sessions;
  active_sessions.reserve(sessions_.size());
  for (const auto& [_, session] : sessions_) {
    active_sessions.push_back(session.get());
  }
  return active_sessions;
}

TransportSessionImpl* TransportSessionRegistryImpl::GetSessionImpl(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = sessions_.find(session_id);
  if (it != sessions_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void TransportSessionRegistryImpl::DestroySession(std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = sessions_.find(session_id);
  if (it != sessions_.end()) {
    sessions_.erase(it);
  }
}

TransportSessionImpl* TransportSessionRegistryImpl::GetOrCreateSession(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TransportSessionImpl* existing_session = GetSessionImpl(session_id);
  if (existing_session) {
    return existing_session;
  }
  return CreateSession(session_id);
}

TransportSessionImpl* TransportSessionRegistryImpl::CreateSession(
    std::string_view session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<TransportSessionImpl> session =
      std::make_unique<TransportSessionImpl>(session_id, channel_);
  TransportSessionImpl* session_ptr = session.get();
  sessions_.emplace(std::string(session_id), std::move(session));
  return session_ptr;
}

void TransportSessionRegistryImpl::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sessions_.clear();
}

base::WeakPtr<TransportSessionRegistryImpl>
TransportSessionRegistryImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace browser_actuator
