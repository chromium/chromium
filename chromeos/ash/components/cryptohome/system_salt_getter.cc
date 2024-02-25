// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/system_salt_getter.h"

#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"

namespace ash {

namespace {

SystemSaltGetter* g_system_salt_getter = nullptr;

}  // namespace

SystemSaltGetter::SystemSaltGetter() = default;

SystemSaltGetter::~SystemSaltGetter() = default;

void SystemSaltGetter::GetSystemSalt(GetSystemSaltCallback callback) {
  if (!system_salt_.empty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), system_salt_));
    return;
  }

  CryptohomeMiscClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&SystemSaltGetter::DidWaitForServiceToBeAvailable,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SystemSaltGetter::AddOnSystemSaltReady(base::OnceClosure closure) {
  if (!raw_salt_.empty()) {
    std::move(closure).Run();
    return;
  }

  on_system_salt_ready_.push_back(std::move(closure));
}

const SystemSaltGetter::RawSalt* SystemSaltGetter::GetRawSalt() const {
  return raw_salt_.empty() ? nullptr : &raw_salt_;
}

void SystemSaltGetter::SetRawSaltForTesting(
    const SystemSaltGetter::RawSalt& raw_salt) {
  raw_salt_ = raw_salt;
}

void SystemSaltGetter::DidWaitForServiceToBeAvailable(
    GetSystemSaltCallback callback,
    bool service_is_available) {
  if (!service_is_available) {
    LOG(ERROR) << "WaitForServiceToBeAvailable failed.";
    std::move(callback).Run(std::string());
    return;
  }
  CryptohomeMiscClient::Get()->GetSystemSalt(
      ::user_data_auth::GetSystemSaltRequest(),
      base::BindOnce(&SystemSaltGetter::DidGetSystemSalt,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SystemSaltGetter::DidGetSystemSalt(
    GetSystemSaltCallback system_salt_callback,
    std::optional<::user_data_auth::GetSystemSaltReply> system_salt_reply) {
  if (system_salt_reply.has_value() && !system_salt_reply->salt().empty() &&
      system_salt_reply->salt().size() % 2 == 0U) {
    raw_salt_ = RawSalt(system_salt_reply->salt().begin(),
                        system_salt_reply->salt().end());
    system_salt_ = ConvertRawSaltToHexString(raw_salt_);

    std::vector<base::OnceClosure> callbacks;
    callbacks.swap(on_system_salt_ready_);
    for (base::OnceClosure& callback : callbacks) {
      std::move(callback).Run();
    }
  } else {
    LOG(WARNING) << "System salt not available";
  }

  std::move(system_salt_callback).Run(system_salt_);
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
  g_system_salt_getter = nullptr;
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
  return base::ToLowerASCII(base::HexEncode(salt));
}

}  // namespace ash
