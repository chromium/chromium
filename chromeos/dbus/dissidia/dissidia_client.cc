// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dissidia/dissidia_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/dissidia/fake_dissidia_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/dissidia/dbus-constants.h"

namespace chromeos {

namespace {

DissidiaClient* g_instance = nullptr;

// Real implementation of DissidiaClient talking to the dissidia-daemon.
class DissidiaClientImpl : public DissidiaClient {
 public:
  DissidiaClientImpl() = default;
  DissidiaClientImpl(const DissidiaClientImpl&) = delete;
  DissidiaClientImpl& operator=(const DissidiaClientImpl&) = delete;
  ~DissidiaClientImpl() override = default;

  void Init(dbus::Bus* bus) {
    proxy_ =
        bus->GetObjectProxy(dissidia::kDissidiaServiceName,
                            dbus::ObjectPath(dissidia::kDissidiaServicePath));

    proxy_->ConnectToSignal(
        dissidia::kDissidiaInterface, dissidia::kProgressSignal,
        base::BindRepeating(&DissidiaClientImpl::OnProgressSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&DissidiaClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));

    proxy_->ConnectToSignal(
        dissidia::kDissidiaInterface, dissidia::kCompletedSignal,
        base::BindRepeating(&DissidiaClientImpl::OnCompletedSignal,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&DissidiaClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void AddObserver(Observer* observer) override {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observers_.RemoveObserver(observer);
  }

  void PerformUpdate(const std::string& target,
                     PerformUpdateCallback callback) override {
    dbus::MethodCall method_call(dissidia::kDissidiaInterface,
                                 dissidia::kPerformUpdateMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(target);
    writer.AppendBool(false);

    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DissidiaClientImpl::OnPerformUpdateResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  void OnPerformUpdateResponse(PerformUpdateCallback callback,
                               dbus::Response* response) {
    if (!response) {
      LOG(ERROR) << "No response received from dissidia PerformUpdate";
      std::move(callback).Run(dissidia::kError,
                              "No response from dissidia daemon.");
      return;
    }

    dbus::MessageReader reader(response);
    int32_t status_value = 0;
    std::string message;
    if (!reader.PopInt32(&status_value) || !reader.PopString(&message)) {
      LOG(ERROR) << "Failed to parse dissidia PerformUpdate response";
      std::move(callback).Run(dissidia::kError, "Failed to parse response.");
      return;
    }

    std::move(callback).Run(
        static_cast<dissidia::PerformUpdateStatus>(status_value), message);
  }

  void OnProgressSignal(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    int32_t percent = 0;
    std::string stage;
    if (!reader.PopInt32(&percent) || !reader.PopString(&stage)) {
      LOG(ERROR) << "Failed to parse dissidia Progress signal";
      return;
    }

    for (auto& observer : observers_) {
      observer.OnProgress(percent, stage);
    }
  }

  void OnCompletedSignal(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    bool success = false;
    int32_t error_code_value = 0;
    std::string message;
    if (!reader.PopBool(&success) || !reader.PopInt32(&error_code_value) ||
        !reader.PopString(&message)) {
      LOG(ERROR) << "Failed to parse dissidia Completed signal";
      return;
    }

    for (auto& observer : observers_) {
      observer.OnCompleted(
          success, static_cast<dissidia::CompletedErrorCode>(error_code_value),
          message);
    }
  }

  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success) {
    if (!success) {
      LOG(ERROR) << "Failed to connect to signal " << interface_name << "."
                 << signal_name;
    }
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;
  base::ObserverList<Observer> observers_;
  base::WeakPtrFactory<DissidiaClientImpl> weak_ptr_factory_{this};
};

}  // namespace

DissidiaClient::DissidiaClient() {
  CHECK(!g_instance);
  g_instance = this;
}

DissidiaClient::~DissidiaClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void DissidiaClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new DissidiaClientImpl())->Init(bus);
}

// static
void DissidiaClient::InitializeFake() {
  new FakeDissidiaClient();
}

// static
void DissidiaClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
DissidiaClient* DissidiaClient::Get() {
  return g_instance;
}

}  // namespace chromeos
