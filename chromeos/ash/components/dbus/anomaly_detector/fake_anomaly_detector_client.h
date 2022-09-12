// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ANOMALY_DETECTOR_FAKE_ANOMALY_DETECTOR_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ANOMALY_DETECTOR_FAKE_ANOMALY_DETECTOR_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/anomaly_detector/anomaly_detector_client.h"

namespace ash {

// FakeAnomalyDetectorClient is a fake implementation of AnomalyDetectorClient
// used for testing.
class COMPONENT_EXPORT(ASH_DBUS_ANOMALY_DETECTOR) FakeAnomalyDetectorClient
    : public AnomalyDetectorClient {
 public:
  FakeAnomalyDetectorClient();
  FakeAnomalyDetectorClient(const FakeAnomalyDetectorClient&) = delete;
  FakeAnomalyDetectorClient& operator=(const FakeAnomalyDetectorClient&) =
      delete;
  ~FakeAnomalyDetectorClient() override;

  // AnomalyDetectorClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool IsGuestFileCorruptionSignalConnected() override;

  void set_guest_file_corruption_signal_connected(bool connected);
  void NotifyGuestFileCorruption(
      const anomaly_detector::GuestFileCorruptionSignal& signal);

 protected:
  void Init(dbus::Bus* bus) override;

 private:
  bool is_container_started_signal_connected_ = true;

  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ANOMALY_DETECTOR_FAKE_ANOMALY_DETECTOR_CLIENT_H_
