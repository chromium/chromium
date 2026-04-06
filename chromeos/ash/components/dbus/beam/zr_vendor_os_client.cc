// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/beam/zr_vendor_os_client.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/beam/fake_zr_vendor_os_client.h"
#include "dbus/bus.h"
#include "dbus/object_proxy.h"

namespace ash {

namespace {

// D-Bus service identifiers matching the zrvendorOs_service configuration.
constexpr char kZrVendorOsServiceName[] =
    "com.google.starline.ZrVendorosService";
constexpr char kZrVendorOsServicePath[] =
    "/com/google/starline/ZrVendorosService";

ZrVendorOsClient* g_instance = nullptr;

class ZrVendorOsClientImpl : public ZrVendorOsClient {
 public:
  explicit ZrVendorOsClientImpl(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(kZrVendorOsServiceName,
                                 dbus::ObjectPath(kZrVendorOsServicePath));
  }

  ~ZrVendorOsClientImpl() override = default;

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    proxy_->WaitForServiceToBeAvailable(std::move(callback));
  }

  void Init(dbus::Bus* const bus) override {}

 private:
  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;
};

}  // namespace

// static
ZrVendorOsClient* ZrVendorOsClient::Get() {
  return g_instance;
}

// static
void ZrVendorOsClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  CHECK(!g_instance);
  g_instance = new ZrVendorOsClientImpl(bus);
  g_instance->Init(bus);
}

// static
void ZrVendorOsClient::InitializeFake() {
  CHECK(!g_instance);
  g_instance = new FakeZrVendorOsClient();
  g_instance->Init(nullptr);
}

// static
void ZrVendorOsClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
  g_instance = nullptr;
}

ZrVendorOsClient::ZrVendorOsClient() {
  CHECK(!g_instance);
  g_instance = this;
}

ZrVendorOsClient::~ZrVendorOsClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
