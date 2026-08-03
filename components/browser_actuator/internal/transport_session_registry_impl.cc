// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/internal/transport_session_registry_impl.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "components/browser_actuator/internal/features.h"
#include "components/browser_actuator/internal/transport_session_impl.h"

namespace browser_actuator {
namespace {

size_t GetMaxConcurrentSessionsLimit() {
  static const size_t limit = []() {
    const int l = kMaxTransportSessions.Get();
    return l > 0 ? static_cast<size_t>(l) : 0;
  }();
  return limit;
}

}  // namespace

TransportSessionRegistryImpl::TransportSessionRegistryImpl(
    base::WeakPtr<TransportChannel> channel)
    : TransportSessionRegistryImpl(std::move(channel),
                                   GetMaxConcurrentSessionsLimit()) {}

TransportSessionRegistryImpl::TransportSessionRegistryImpl(
    base::WeakPtr<TransportChannel> channel,
    size_t max_concurrent_sessions)
    : channel_(std::move(channel)),
      max_concurrent_sessions_(max_concurrent_sessions) {
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
  base::UmaHistogramExactLinear(
      "Browser.Actuator.SessionRegistry.ExistingSessionsCount",
      static_cast<int>(sessions_.size()),
      /*exclusive_max=*/20);
  const bool limit_reached = sessions_.size() >= max_concurrent_sessions_;
  base::UmaHistogramBoolean(
      "Browser.Actuator.SessionRegistry.SessionLimitReached", limit_reached);
  if (limit_reached) {
    DLOG(WARNING) << "Max concurrent sessions limit ("
                  << max_concurrent_sessions_
                  << ") reached. Rejecting new session.";
    return nullptr;
  }
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
