// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_observer.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/fake_cfm_hotline_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

CfmHotlineClient* g_instance = nullptr;

class CfmHotlineClientImpl : public CfmHotlineClient {
 public:
  CfmHotlineClientImpl() = default;
  CfmHotlineClientImpl(const CfmHotlineClientImpl&) = delete;
  CfmHotlineClientImpl& operator=(const CfmHotlineClientImpl&) = delete;
  ~CfmHotlineClientImpl() override = default;

  void Init(dbus::Bus* const bus) {
    dbus_proxy_ =
        bus->GetObjectProxy(::cfm::broker::kServiceName,
                            dbus::ObjectPath(::cfm::broker::kServicePath));

    dbus_proxy_->ConnectToSignal(
        ::cfm::broker::kServiceInterfaceName,
        ::cfm::broker::kMojoServiceRequestedSignal,
        base::BindRepeating(
            &CfmHotlineClientImpl::OnServiceRequestedSignalReceived,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(&CfmHotlineClientImpl::OnSignalConnected,
                            weak_ptr_factory_.GetWeakPtr()));
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    dbus_proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void BootstrapMojoConnection(
      base::ScopedFD fd,
      BootstrapMojoConnectionCallback result_callback) override {
    dbus::MethodCall method_call(::cfm::broker::kServiceInterfaceName,
                                 ::cfm::broker::kBootstrapMojoConnectionMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(/*is_outgoing_invitation=*/true);
    writer.AppendFileDescriptor(fd.get());
    dbus_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&CfmHotlineClientImpl::OnBootstrapMojoConnectionResponse,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(result_callback)));
  }

  void AddObserver(cfm::CfmObserver* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(cfm::CfmObserver* observer) override {
    observer_list_.RemoveObserver(observer);
  }

 private:
  void OnServiceRequestedSignalReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    std::string service_id;

    if (!reader.PopString(&service_id)) {
      LOG(ERROR) << "Invalid detection signal: " << signal->ToString();
      return;
    }

    for (auto& observer : observer_list_) {
      if (observer.ServiceRequestReceived(service_id)) {
        // A service has been found that can fulfill the request
        // Note: Only one service will match the requested service_id.
        break;
      }
    }
  }

  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool succeeded) {
    LOG_IF(ERROR, !succeeded)
        << "Connection to " << interface << " " << signal << " failed.";
  }

  // Passes the invitation token of |dbus_response| on to |result_callback|.
  void OnBootstrapMojoConnectionResponse(
      BootstrapMojoConnectionCallback result_callback,
      dbus::Response* const response) {
    std::move(result_callback).Run(response != nullptr);
  }

  raw_ptr<dbus::ObjectProxy> dbus_proxy_ = nullptr;
  cfm::CfmObserverList observer_list_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CfmHotlineClientImpl> weak_ptr_factory_{this};
};

}  // namespace

CfmHotlineClient::CfmHotlineClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

CfmHotlineClient::~CfmHotlineClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void CfmHotlineClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new CfmHotlineClientImpl())->Init(bus);
}

// static
void CfmHotlineClient::InitializeFake() {
  new FakeCfmHotlineClient();
}

// static
void CfmHotlineClient::Shutdown() {
  if (g_instance) {
    delete g_instance;
  }
}

// static
bool CfmHotlineClient::IsInitialized() {
  return g_instance;
}

// static
CfmHotlineClient* CfmHotlineClient::Get() {
  CHECK(IsInitialized());
  return g_instance;
}

}  // namespace ash
