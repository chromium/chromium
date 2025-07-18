// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/tab_handle_factory.h"

#include "base/no_destructor.h"
#include "base/types/pass_key.h"

namespace tabs {

SessionMappedTabHandleFactory& SessionMappedTabHandleFactory::GetInstance() {
  static base::NoDestructor<SessionMappedTabHandleFactory> instance;
  return *instance;
}

SessionMappedTabHandleFactory::SessionMappedTabHandleFactory() = default;
SessionMappedTabHandleFactory::~SessionMappedTabHandleFactory() = default;

void SessionMappedTabHandleFactory::SetSessionIdForHandle(
    base::PassKey<SupportsTabHandles>,
    int32_t handle_value,
    int32_t session_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence());

  // A handle value of 0 is null and should not be used.
  if (handle_value == SupportsTabHandles::Handle::NullValue) {
    return;
  }

  // Clean up any old mapping for this handle.
  if (auto it = handle_value_to_session_id_.find(handle_value);
      it != handle_value_to_session_id_.end()) {
    session_id_to_handle_value_.erase(it->second);
    handle_value_to_session_id_.erase(it);
  }

  // Clean up any old mapping for this session ID.
  if (auto it = session_id_to_handle_value_.find(session_id);
      it != session_id_to_handle_value_.end()) {
    handle_value_to_session_id_.erase(it->second);
  }
  session_id_to_handle_value_[session_id] = handle_value;
  handle_value_to_session_id_.emplace(handle_value, session_id);
}

void SessionMappedTabHandleFactory::ClearHandleMappings(
    base::PassKey<SupportsTabHandles>,
    int32_t handle_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence());
  if (auto it = handle_value_to_session_id_.find(handle_value);
      it != handle_value_to_session_id_.end()) {
    session_id_to_handle_value_.erase(it->second);
    handle_value_to_session_id_.erase(it);
  }
}

int32_t SessionMappedTabHandleFactory::GetHandleForSessionId(
    int32_t session_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence());
  const auto it = session_id_to_handle_value_.find(session_id);
  return it != session_id_to_handle_value_.end()
             ? it->second
             : SupportsTabHandles::Handle::NullValue;
}

std::optional<int32_t> SessionMappedTabHandleFactory::GetSessionIdForHandle(
    int32_t handle) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence());
  const auto it = handle_value_to_session_id_.find(handle);
  return it != handle_value_to_session_id_.end()
             ? std::make_optional(it->second)
             : std::nullopt;
}

void SessionMappedTabHandleFactory::OnHandleFreed(int32_t handle_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence());

  if (auto it = handle_value_to_session_id_.find(handle_value);
      it != handle_value_to_session_id_.end()) {
    session_id_to_handle_value_.erase(it->second);
    handle_value_to_session_id_.erase(it);
  }
}

void SupportsTabHandles::SetSessionId(int32_t session_id) {
  SessionMappedTabHandleFactory::GetInstance().SetSessionIdForHandle(
      base::PassKey<SupportsTabHandles>(), GetHandle().raw_value(), session_id);
}

void SupportsTabHandles::ClearSessionId() {
  SessionMappedTabHandleFactory::GetInstance().ClearHandleMappings(
      base::PassKey<SupportsTabHandles>(), GetHandle().raw_value());
}

}  // namespace tabs
