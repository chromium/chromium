// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ANOMALY_DETECTOR_ANOMALY_DETECTOR_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ANOMALY_DETECTOR_ANOMALY_DETECTOR_CLIENT_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/anomaly_detector/anomaly_detector.pb.h"
#include "chromeos/dbus/common/dbus_client.h"

namespace ash {

// AnomalyDetectorClient is used to communicate with anomaly_detector.
// Currently this just amounts to listening to signals it sends.
class COMPONENT_EXPORT(ASH_DBUS_ANOMALY_DETECTOR) AnomalyDetectorClient
    : public chromeos::DBusClient {
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

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance if it has been initialized.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static AnomalyDetectorClient* Get();

 protected:
  // Initialize() should be used instead.
  AnomalyDetectorClient();
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ANOMALY_DETECTOR_ANOMALY_DETECTOR_CLIENT_H_
