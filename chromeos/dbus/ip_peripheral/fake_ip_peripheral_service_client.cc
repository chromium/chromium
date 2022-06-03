// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ip_peripheral/fake_ip_peripheral_service_client.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {
namespace {

FakeIpPeripheralServiceClient* g_instance = nullptr;

constexpr int32_t kMinPan = -400;
constexpr int32_t kMaxPan = 400;
constexpr int32_t kMinTilt = -300;
constexpr int32_t kMaxTilt = 300;
constexpr int32_t kMinZoom = 100;
constexpr int32_t kMaxZoom = 500;

}  // namespace

// static
FakeIpPeripheralServiceClient* FakeIpPeripheralServiceClient::Get() {
  CHECK(g_instance);
  return g_instance;
}

FakeIpPeripheralServiceClient::FakeIpPeripheralServiceClient() {
  CHECK(!g_instance);
  g_instance = this;
}

FakeIpPeripheralServiceClient::~FakeIpPeripheralServiceClient() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void FakeIpPeripheralServiceClient::GetPan(const std::string& ip,
                                           GetCallback callback) {
  get_pan_call_count_++;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), true, pan_, kMinPan, kMaxPan));
}

void FakeIpPeripheralServiceClient::GetTilt(const std::string& ip,
                                            GetCallback callback) {
  get_tilt_call_count_++;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), true, tilt_, kMinTilt, kMaxTilt));
}

void FakeIpPeripheralServiceClient::GetZoom(const std::string& ip,
                                            GetCallback callback) {
  get_zoom_call_count_++;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), true, zoom_, kMinZoom, kMaxZoom));
}

void FakeIpPeripheralServiceClient::SetPan(const std::string& ip,
                                           int32_t pan,
                                           SetCallback callback) {
  set_pan_call_count_++;
  pan_ = pan;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeIpPeripheralServiceClient::SetTilt(const std::string& ip,
                                            int32_t tilt,
                                            SetCallback callback) {
  set_tilt_call_count_++;
  tilt_ = tilt;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeIpPeripheralServiceClient::SetZoom(const std::string& ip,
                                            int32_t zoom,
                                            SetCallback callback) {
  set_zoom_call_count_++;
  zoom_ = zoom;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

}  // namespace chromeos
