// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_ANOMALY_DETECTOR_ANOMALY_DETECTOR_CLIENT_H_
#define CHROMEOS_DBUS_ANOMALY_DETECTOR_ANOMALY_DETECTOR_CLIENT_H_

#include <memory>

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/dbus/anomaly_detector/anomaly_detector.pb.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace chromeos {

// AnomalyDetectorClient is used to communicate with anomaly_detector.
// Currently this just amounts to listening to signals it sends.
class COMPONENT_EXPORT(CHROMEOS_DBUS_ANOMALY_DETECTOR) AnomalyDetectorClient
    : public DBusClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // OnGuestFileCorruption is signaled by anomaly_detector when it detects
    // that a VM has encountered filesystem corruption.
    virtual void OnGuestFileCorruption(
        const anomaly_detector::GuestFileCorruptionSignal& signal) = 0;
  };

  AnomalyDetectorClient(const AnomalyDetectorClient&) = delete;
  AnomalyDetectorClient& operator=(const AnomalyDetectorClient&) = delete;
  ~AnomalyDetectorClient() override;

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

  // IsGuestFileCorruptionSignalConnected must return true before starting a VM,
  // or we will be unable to detect if it's filesystem is corrupt.
  virtual bool IsGuestFileCorruptionSignalConnected() = 0;

  // Creates an instance of AnomalyDetectorClient.
  static std::unique_ptr<AnomalyDetectorClient> Create();

 protected:
  // Create() should be used instead.
  AnomalyDetectorClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_ANOMALY_DETECTOR_ANOMALY_DETECTOR_CLIENT_H_
