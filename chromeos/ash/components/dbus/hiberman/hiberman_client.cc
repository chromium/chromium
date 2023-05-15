// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/hiberman/hiberman_client.h"

#include <utility>

#include <google/protobuf/message_lite.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/hiberman/fake_hiberman_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/hiberman/dbus-constants.h"

namespace ash {
namespace {

// The default time for the resume from hibernate method call. This method
// deliberately blocks the rest of boot while the (potentially large) hibernate
// image is being loaded and prepared. As a worst case estimate, loading 8GB at
// 100MB/s takes about 80 seconds. 5 minutes would give us a fudge factor of
// roughly 4x.
constexpr int kHibermanResumeTimeoutMs = 5 * 60 * 1000;

HibermanClient* g_instance = nullptr;

// "Real" implementation of HibermanClient talking to the hiberman's
// ResumeInterface interface on the Chrome OS side.
class HibermanClientImpl : public HibermanClient {
 public:
  HibermanClientImpl() = default;
  ~HibermanClientImpl() override = default;

  // Not copyable or movable.
  HibermanClientImpl(const HibermanClientImpl&) = delete;
  HibermanClientImpl& operator=(const HibermanClientImpl&) = delete;

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(
        ::hiberman::kHibernateServiceName,
        dbus::ObjectPath(::hiberman::kHibernateServicePath));

    WaitForServiceToBeAvailable(
        base::BindOnce(&HibermanClientImpl::WaitAvailableCallback,
                       weak_factory_.GetWeakPtr()));
  }

  // HibermanClient override:
  bool IsAlive() const override { return alive_; }

  void WaitForServiceToBeAvailable(
      chromeos::WaitForServiceToBeAvailableCallback callback) override {
    VLOG(1) << "Start WaitForServiceToBeAvailable";
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void ResumeFromHibernate(const std::string& account_id,
                           ResumeFromHibernateCallback callback) override {
    VLOG(1) << "Attempt ResumeFromHibernate";
    dbus::MethodCall method_call(::hiberman::kHibernateResumeInterface,
                                 ::hiberman::kResumeFromHibernateMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(account_id);
    // Bind with the weak pointer of |this| so the response is not
    // handled once |this| is already destroyed.
    proxy_->CallMethod(
        &method_call, kHibermanResumeTimeoutMs,
        base::BindOnce(&HibermanClientImpl::HandleResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ResumeFromHibernateAS(const std::string& auth_session_id,
                             ResumeFromHibernateCallback callback) override {
    VLOG(1) << "Attempt ResumeFromHibernateAS";
    dbus::MethodCall method_call(::hiberman::kHibernateResumeInterface,
                                 ::hiberman::kResumeFromHibernateASMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfBytes(
        reinterpret_cast<const uint8_t*>(&auth_session_id[0]),
        auth_session_id.length());
    // Bind with the weak pointer of |this| so the response is not
    // handled once |this| is already destroyed.
    proxy_->CallMethod(
        &method_call, kHibermanResumeTimeoutMs,
        base::BindOnce(&HibermanClientImpl::HandleResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  void HandleResponse(chromeos::VoidDBusMethodCallback callback,
                      dbus::Response* response) {
    VLOG(1) << "Received Resume Response: " << (response != nullptr);
    std::move(callback).Run(response != nullptr);
  }

  void WaitAvailableCallback(bool service_is_available) {
    alive_ = service_is_available;
  }

  // We need to test if hiberman is responding on the dbus interface, we do this
  // to make sure we don't block login unnecessarily.
  bool alive_ = false;

  // D-Bus proxy for hiberman, not owned.
  raw_ptr<dbus::ObjectProxy, ExperimentalAsh> proxy_ = nullptr;
  base::WeakPtrFactory<HibermanClientImpl> weak_factory_{this};
};

}  // namespace

HibermanClient::HibermanClient() {
  CHECK(!g_instance);
  g_instance = this;
}

HibermanClient::~HibermanClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void HibermanClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new HibermanClientImpl())->Init(bus);
}

// static
void HibermanClient::InitializeFake() {
  // Certain tests may create FakeHibermanClient() before the browser starts
  // to set parameters.
  if (!FakeHibermanClient::Get()) {
    new FakeHibermanClient();
  }
}

// static
void HibermanClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  // The destructor resets |g_instance|.
  DCHECK(!g_instance);
}

// static
HibermanClient* HibermanClient::Get() {
  return g_instance;
}

}  // namespace ash
