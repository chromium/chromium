// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_SESSION_REGISTRY_IMPL_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_SESSION_REGISTRY_IMPL_H_

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/browser_actuator/public/transport_session_registry.h"

namespace browser_actuator {

class TransportChannel;
class TransportSessionImpl;

// Manages the lifecycle of active transport sessions. Owned by the
// TransportChannel.
class TransportSessionRegistryImpl : public TransportSessionRegistry {
 public:
  explicit TransportSessionRegistryImpl(
      base::WeakPtr<TransportChannel> channel);
  TransportSessionRegistryImpl(base::WeakPtr<TransportChannel> channel,
                               size_t max_concurrent_sessions);
  ~TransportSessionRegistryImpl() override;

  TransportSessionRegistryImpl(const TransportSessionRegistryImpl&) = delete;
  TransportSessionRegistryImpl& operator=(const TransportSessionRegistryImpl&) =
      delete;

  // TransportSessionRegistry implementation.
  TransportSession* GetSession(std::string_view session_id) override;

  // Concrete methods for session lookup and management.
  TransportSessionImpl* GetSessionImpl(std::string_view session_id);
  TransportSessionImpl* GetOrCreateSession(std::string_view session_id);
  void DestroySession(std::string_view session_id);
  std::vector<TransportSessionImpl*> GetAllSessionImpls();

  // Clears all active sessions from the sessions_ map.
  void Clear();

  size_t max_concurrent_sessions() const { return max_concurrent_sessions_; }

  base::WeakPtr<TransportSessionRegistryImpl> GetWeakPtr();

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  TransportSessionImpl* CreateSession(std::string_view session_id);

  base::WeakPtr<TransportChannel> channel_;
  const size_t max_concurrent_sessions_;

  using SessionMap =
      base::flat_map<std::string,                            // Session ID
                     std::unique_ptr<TransportSessionImpl>,  // Session instance
                     std::less<>>;                           // comparator
  SessionMap sessions_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<TransportSessionRegistryImpl> weak_ptr_factory_{this};
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_SESSION_REGISTRY_IMPL_H_
