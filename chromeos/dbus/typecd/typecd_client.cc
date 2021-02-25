// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/typecd/typecd_client.h"

#include "chromeos/dbus/typecd/fake_typecd_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"

namespace chromeos {

namespace {
TypecdClient* g_instance = nullptr;
}  // namespace

class TypecdClientImpl : public TypecdClient {
 public:
  TypecdClientImpl() = default;
  TypecdClientImpl(const TypecdClientImpl&) = delete;
  TypecdClientImpl& operator=(const TypecdClientImpl&) = delete;
  ~TypecdClientImpl() override = default;

  void Init(dbus::Bus* bus) {
    // TODO(jimmyxgong): Initialize this client with the proper D-bus service
    // path.
  }

 private:
  void ThunderboltDeviceConnectedReceived(dbus::Signal* signal) {
    // TODO(jimmyxgong): Implement this when the signal is generated.
    NOTIMPLEMENTED();
  }
};

TypecdClient::TypecdClient() {
  CHECK(!g_instance);
  g_instance = this;
}

TypecdClient::~TypecdClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void TypecdClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new TypecdClientImpl())->Init(bus);
}

// static
void TypecdClient::InitializeFake() {
  new FakeTypecdClient();
}

// static
void TypecdClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
TypecdClient* TypecdClient::Get() {
  return g_instance;
}

}  // namespace chromeos
