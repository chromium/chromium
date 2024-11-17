// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/on_device_translation/sandbox_hook.h"

#include "components/services/on_device_translation/translate_kit_client.h"
#include "sandbox/linux/syscall_broker/broker_command.h"

using sandbox::syscall_broker::BrokerFilePermission;
using sandbox::syscall_broker::MakeBrokerCommandSet;

namespace on_device_translation {
namespace {

// Gets the file permissions required by the TranslateKit
std::vector<BrokerFilePermission> GetOnDeviceTranslationFilePermissions() {
  std::vector<BrokerFilePermission> permissions{
      // Opened for checking the CPU numbers.
      BrokerFilePermission::ReadOnly("/sys/devices/system/cpu/possible"),
  };
  return permissions;
}

}  // namespace

bool OnDeviceTranslationSandboxHook(
    sandbox::policy::SandboxLinux::Options options) {
  // Call `TranslateKitClient::Get()` to load libtranslatekit.so
  TranslateKitClient::Get();

  auto* instance = sandbox::policy::SandboxLinux::GetInstance();
  instance->StartBrokerProcess(MakeBrokerCommandSet({
                                   sandbox::syscall_broker::COMMAND_OPEN,
                               }),
                               GetOnDeviceTranslationFilePermissions(),
                               options);
  instance->EngageNamespaceSandboxIfPossible();

  return true;
}

}  // namespace on_device_translation
