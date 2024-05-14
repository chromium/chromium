// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/anomaly_detector/anomaly_detector_client.h"

#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/anomaly_detector/fake_anomaly_detector_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/anomaly_detector/dbus-constants.h"

namespace ash {
namespace {

AnomalyDetectorClient* g_instance = nullptr;

}  // namespace

class AnomalyDetectorClientImpl : public AnomalyDetectorClient {
 public:
  AnomalyDetectorClientImpl() = default;
  AnomalyDetectorClientImpl(const AnomalyDetectorClientImpl&) = delete;
  AnomalyDetectorClientImpl& operator=(const AnomalyDetectorClientImpl&) =
      delete;
  ~AnomalyDetectorClientImpl() override = default;

  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  bool IsGuestFileCorruptionSignalConnected() override {
    return is_guest_file_corruption_signal_connected_;
  }

  void Init(dbus::Bus* bus) override {
    anomaly_detector_proxy_ = bus->GetObjectProxy(
        anomaly_detector::kAnomalyEventServiceName,
        dbus::ObjectPath(anomaly_detector::kAnomalyEventServicePath));
    if (!anomaly_detector_proxy_) {
      LOG(ERROR) << "Unable to get dbus proxy for "
                 << anomaly_detector::kAnomalyEventServiceName;
    }
    anomaly_detector_proxy_->ConnectToSignal(
        anomaly_detector::kAnomalyEventServiceInterface,
        anomaly_detector::kAnomalyGuestFileCorruptionSignalName,
        base::BindRepeating(
            &AnomalyDetectorClientImpl::OnGuestFileCorruptionSignal,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&AnomalyDetectorClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnGuestFileCorruptionSignal(dbus::Signal* signal) {
    anomaly_detector::GuestFileCorruptionSignal proto_signal;
    dbus::MessageReader reader(signal);
    if (!reader.PopArrayOfBytesAsProto(&proto_signal)) {
      LOG(ERROR) << "Failed to parse proto from DBus Signal";
      return;
    }
    for (auto& observer : observer_list_) {
      observer.OnGuestFileCorruption(proto_signal);
    }
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool is_connected) {
    if (!is_connected) {
      LOG(ERROR) << "Failed to connect to signal " << signal_name;
    }
    DCHECK_EQ(interface_name, anomaly_detector::kAnomalyEventServiceInterface);
    if (signal_name ==
        anomaly_detector::kAnomalyGuestFileCorruptionSignalName) {
      is_guest_file_corruption_signal_connected_ = is_connected;
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  raw_ptr<dbus::ObjectProxy> anomaly_detector_proxy_ = nullptr;

  base::ObserverList<Observer> observer_list_;

  bool is_guest_file_corruption_signal_connected_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<AnomalyDetectorClientImpl> weak_ptr_factory_{this};
};

AnomalyDetectorClient::AnomalyDetectorClient() {
  CHECK(!g_instance);
  g_instance = this;
}

AnomalyDetectorClient::~AnomalyDetectorClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
void AnomalyDetectorClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new AnomalyDetectorClientImpl())->Init(bus);
}

// static
void AnomalyDetectorClient::InitializeFake() {
  new FakeAnomalyDetectorClient();
}

// static
void AnomalyDetectorClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
AnomalyDetectorClient* AnomalyDetectorClient::Get() {
  return g_instance;
}

}  // namespace ash
