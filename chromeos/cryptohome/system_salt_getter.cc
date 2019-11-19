// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/cryptohome/system_salt_getter.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"

namespace chromeos {
namespace {

SystemSaltGetter* g_system_salt_getter = NULL;

}  // namespace

SystemSaltGetter::SystemSaltGetter() {}

SystemSaltGetter::~SystemSaltGetter() = default;

void SystemSaltGetter::GetSystemSalt(
    const GetSystemSaltCallback& callback) {
  if (!system_salt_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, system_salt_));
    return;
  }

  CryptohomeClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&SystemSaltGetter::DidWaitForServiceToBeAvailable,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void SystemSaltGetter::AddOnSystemSaltReady(const base::Closure& closure) {
  if (!raw_salt_.empty()) {
    closure.Run();
    return;
  }

  on_system_salt_ready_.push_back(closure);
}

const SystemSaltGetter::RawSalt* SystemSaltGetter::GetRawSalt() const {
  return raw_salt_.empty() ? nullptr : &raw_salt_;
}

void SystemSaltGetter::SetRawSaltForTesting(
    const SystemSaltGetter::RawSalt& raw_salt) {
  raw_salt_ = raw_salt;
}

void SystemSaltGetter::DidWaitForServiceToBeAvailable(
    const GetSystemSaltCallback& callback,
    bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "WaitForServiceToBeAvailable failed.";
    callback.Run(std::string());
    return;
  }
  CryptohomeClient::Get()->GetSystemSalt(
      base::BindOnce(&SystemSaltGetter::DidGetSystemSalt,
                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void SystemSaltGetter::DidGetSystemSalt(
    const GetSystemSaltCallback& callback,
    base::Optional<std::vector<uint8_t>> system_salt) {
  if (system_salt.has_value() && !system_salt->empty() &&
      system_salt->size() % 2 == 0U) {
    raw_salt_ = std::move(system_salt).value();
    system_salt_ = ConvertRawSaltToHexString(raw_salt_);

    std::vector<base::Closure> callbacks;
    callbacks.swap(on_system_salt_ready_);
    for (const base::Closure& callback : callbacks) {
      callback.Run();
    }
  } else {
    LOG(WARNING) << "System salt not available";
  }

  callback.Run(system_salt_);
}

// static
void SystemSaltGetter::Initialize() {
  CHECK(!g_system_salt_getter);
  g_system_salt_getter = new SystemSaltGetter();
}

// static
bool SystemSaltGetter::IsInitialized() {
  return g_system_salt_getter;
}

// static
void SystemSaltGetter::Shutdown() {
  CHECK(g_system_salt_getter);
  delete g_system_salt_getter;
  g_system_salt_getter = NULL;
}

// static
SystemSaltGetter* SystemSaltGetter::Get() {
  CHECK(g_system_salt_getter)
      << "SystemSaltGetter::Get() called before Initialize()";
  return g_system_salt_getter;
}

// static
std::string SystemSaltGetter::ConvertRawSaltToHexString(
    const std::vector<uint8_t>& salt) {
  return base::ToLowerASCII(
      base::HexEncode(reinterpret_cast<const void*>(salt.data()), salt.size()));
}

}  // namespace chromeos
