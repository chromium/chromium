// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/disconnect_tethering_request_sender.h"

namespace ash {

namespace tether {

DisconnectTetheringRequestSender::DisconnectTetheringRequestSender() = default;

DisconnectTetheringRequestSender::~DisconnectTetheringRequestSender() = default;

void DisconnectTetheringRequestSender::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void DisconnectTetheringRequestSender::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void DisconnectTetheringRequestSender::
    NotifyPendingDisconnectRequestsComplete() {
  for (auto& observer : observer_list_)
    observer.OnPendingDisconnectRequestsComplete();
}

}  // namespace tether

}  // namespace ash
