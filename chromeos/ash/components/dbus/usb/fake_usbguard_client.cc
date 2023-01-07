// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/usb/fake_usbguard_client.h"

namespace ash {

namespace {

FakeUsbguardClient* g_instance = nullptr;

}  // namespace

FakeUsbguardClient::FakeUsbguardClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakeUsbguardClient::~FakeUsbguardClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}
// static
FakeUsbguardClient* FakeUsbguardClient::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void FakeUsbguardClient::AddObserver(UsbguardObserver* observer) {
  observers_.AddObserver(observer);
}

void FakeUsbguardClient::RemoveObserver(UsbguardObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool FakeUsbguardClient::HasObserver(const UsbguardObserver* observer) const {
  return observers_.HasObserver(observer);
}

void FakeUsbguardClient::SendDevicePolicyChanged(
    uint32_t id,
    UsbguardObserver::Target target_old,
    UsbguardObserver::Target target_new,
    const std::string& device_rule,
    uint32_t rule_id,
    const std::map<std::string, std::string>& attributes) {
  for (auto& observer : observers_) {
    observer.DevicePolicyChanged(id, target_old, target_new, device_rule,
                                 rule_id, attributes);
  }
}

}  // namespace ash
