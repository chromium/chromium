// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/oobe_config/fake_oobe_configuration_client.h"

#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

std::string LoadConfigurationFile(base::FilePath path) {
  std::string configuration_data;
  if (!base::ReadFileToString(path, &configuration_data)) {
    DLOG(WARNING) << "Can't read OOBE Configuration";
    return std::string();
  }
  return configuration_data;
}

void OnConfigurationLoaded(
    ash::OobeConfigurationClient::ConfigurationCallback callback,
    const std::string& configuration) {
  std::move(callback).Run(!configuration.empty(), configuration);
}

}  // namespace

namespace ash {

FakeOobeConfigurationClient::FakeOobeConfigurationClient() = default;

FakeOobeConfigurationClient::~FakeOobeConfigurationClient() = default;

void FakeOobeConfigurationClient::Init(dbus::Bus* bus) {}

void FakeOobeConfigurationClient::CheckForOobeConfiguration(
    ConfigurationCallback callback) {
  if (configuration_.has_value()) {
    std::move(callback).Run(true, *configuration_);
    return;
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kFakeOobeConfiguration)) {
    std::move(callback).Run(false, std::string());
    return;
  }

  const base::FilePath path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          chromeos::switches::kFakeOobeConfiguration);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&LoadConfigurationFile, path),
      base::BindOnce(&OnConfigurationLoaded, std::move(callback)));
}

void FakeOobeConfigurationClient::SetConfiguration(
    const std::string& configuration) {
  CHECK(!configuration.empty());
  configuration_ = configuration;
}

}  // namespace ash
