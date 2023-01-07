// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/anomaly_detector/fake_anomaly_detector_client.h"

namespace ash {

FakeAnomalyDetectorClient::FakeAnomalyDetectorClient() = default;
FakeAnomalyDetectorClient::~FakeAnomalyDetectorClient() = default;
void FakeAnomalyDetectorClient::Init(dbus::Bus* bus) {}

void FakeAnomalyDetectorClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakeAnomalyDetectorClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

bool FakeAnomalyDetectorClient::IsGuestFileCorruptionSignalConnected() {
  return is_container_started_signal_connected_;
}

void FakeAnomalyDetectorClient::set_guest_file_corruption_signal_connected(
    bool connected) {
  is_container_started_signal_connected_ = connected;
}

void FakeAnomalyDetectorClient::NotifyGuestFileCorruption(
    const anomaly_detector::GuestFileCorruptionSignal& signal) {
  for (auto& observer : observer_list_) {
    observer.OnGuestFileCorruption(signal);
  }
}

}  // namespace ash
