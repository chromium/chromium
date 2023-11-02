// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_disconnect_tethering_request_sender.h"

namespace ash {

namespace tether {

FakeDisconnectTetheringRequestSender::FakeDisconnectTetheringRequestSender() =
    default;

FakeDisconnectTetheringRequestSender::~FakeDisconnectTetheringRequestSender() =
    default;

void FakeDisconnectTetheringRequestSender::SendDisconnectRequestToDevice(
    const std::string& device_id) {
  device_ids_sent_requests_.push_back(device_id);
}

bool FakeDisconnectTetheringRequestSender::HasPendingRequests() {
  return has_pending_requests_;
}

void FakeDisconnectTetheringRequestSender::
    NotifyPendingDisconnectRequestsComplete() {
  DisconnectTetheringRequestSender::NotifyPendingDisconnectRequestsComplete();
}

}  // namespace tether

}  // namespace ash
