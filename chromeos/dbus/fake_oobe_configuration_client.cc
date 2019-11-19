// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_oobe_configuration_client.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "chromeos/dbus/constants/dbus_switches.h"

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
    chromeos::OobeConfigurationClient::ConfigurationCallback callback,
    const std::string& configuration) {
  std::move(callback).Run(!configuration.empty(), configuration);
}

}  // namespace

namespace chromeos {

FakeOobeConfigurationClient::FakeOobeConfigurationClient() = default;

FakeOobeConfigurationClient::~FakeOobeConfigurationClient() = default;

void FakeOobeConfigurationClient::Init(dbus::Bus* bus) {}

void FakeOobeConfigurationClient::CheckForOobeConfiguration(
    ConfigurationCallback callback) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kFakeOobeConfiguration)) {
    std::move(callback).Run(false, std::string());
    return;
  }

  const base::FilePath path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          chromeos::switches::kFakeOobeConfiguration);

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&LoadConfigurationFile, path),
      base::BindOnce(&OnConfigurationLoaded, std::move(callback)));
}

}  // namespace chromeos
