// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_pending_connection_request_delegate.h"

namespace ash::secure_channel {

FakePendingConnectionRequestDelegate::FakePendingConnectionRequestDelegate() =
    default;

FakePendingConnectionRequestDelegate::~FakePendingConnectionRequestDelegate() =
    default;

const std::optional<PendingConnectionRequestDelegate::FailedConnectionReason>&
FakePendingConnectionRequestDelegate::GetFailedConnectionReasonForId(
    const base::UnguessableToken& request_id) {
  return request_id_to_failed_connection_reason_map_[request_id];
}

void FakePendingConnectionRequestDelegate::OnRequestFinishedWithoutConnection(
    const base::UnguessableToken& request_id,
    FailedConnectionReason reason) {
  request_id_to_failed_connection_reason_map_[request_id] = reason;

  if (closure_for_next_delegate_callback_)
    std::move(closure_for_next_delegate_callback_).Run();
}

}  // namespace ash::secure_channel
