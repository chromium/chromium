// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/gnubby_client.h"

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

class GnubbyClientImpl : public GnubbyClient {
 public:
  GnubbyClientImpl() {}

  // GnubbyClient override.
  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  // GnubbyClient override.
  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

  void PromptUserAuth(dbus::Signal* signal) {
    for (auto& observer : observer_list_)
      observer.PromptUserAuth();
  }

 protected:
  void Init(dbus::Bus* bus) override {
    dbus::ObjectProxy* proxy_ = bus->GetObjectProxy(
        u2f::kU2FServiceName, dbus::ObjectPath(u2f::kU2FServicePath));

    proxy_->ConnectToSignal(
        u2f::kU2FInterface, u2f::kU2FUserNotificationSignal,
        base::BindRepeating(&GnubbyClientImpl::PromptUserAuth,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&GnubbyClientImpl::OnSignalConnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded) {
    LOG_IF(ERROR, !succeeded)
        << "Connect to " << interface << " " << signal << " failed.";
  }

  base::ObserverList<Observer>::Unchecked observer_list_;

  base::WeakPtrFactory<GnubbyClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GnubbyClientImpl);
};

}  // namespace

GnubbyClient::GnubbyClient() = default;

GnubbyClient::~GnubbyClient() = default;

// static
std::unique_ptr<GnubbyClient> GnubbyClient::Create() {
  return std::make_unique<GnubbyClientImpl>();
}
}  // namespace chromeos
