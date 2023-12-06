// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_DELEGATE_H_

#include <optional>
#include <unordered_map>

#include "base/functional/callback.h"
#include "chromeos/ash/services/secure_channel/pending_connection_request_delegate.h"

namespace ash::secure_channel {

// Test PendingConnectionRequestDelegate implementation.
class FakePendingConnectionRequestDelegate
    : public PendingConnectionRequestDelegate {
 public:
  FakePendingConnectionRequestDelegate();

  FakePendingConnectionRequestDelegate(
      const FakePendingConnectionRequestDelegate&) = delete;
  FakePendingConnectionRequestDelegate& operator=(
      const FakePendingConnectionRequestDelegate&) = delete;

  ~FakePendingConnectionRequestDelegate() override;

  const std::optional<FailedConnectionReason>& GetFailedConnectionReasonForId(
      const base::UnguessableToken& request_id);

  void set_closure_for_next_delegate_callback(base::OnceClosure closure) {
    closure_for_next_delegate_callback_ = std::move(closure);
  }

 private:
  // PendingConnectionRequestDelegate:
  void OnRequestFinishedWithoutConnection(
      const base::UnguessableToken& request_id,
      FailedConnectionReason reason) override;

  std::unordered_map<base::UnguessableToken,
                     std::optional<FailedConnectionReason>,
                     base::UnguessableTokenHash>
      request_id_to_failed_connection_reason_map_;

  base::OnceClosure closure_for_next_delegate_callback_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_PENDING_CONNECTION_REQUEST_DELEGATE_H_
