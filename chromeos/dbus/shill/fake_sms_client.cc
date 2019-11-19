// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill/fake_sms_client.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "dbus/object_path.h"

namespace chromeos {

FakeSMSClient::FakeSMSClient() = default;

FakeSMSClient::~FakeSMSClient() = default;

void FakeSMSClient::GetAll(const std::string& service_name,
                           const dbus::ObjectPath& object_path,
                           GetAllCallback callback) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kSmsTestMessages))
    return;

  // Ownership passed to callback
  base::DictionaryValue sms;
  sms.SetString("Number", "000-000-0000");
  sms.SetString("Text", "FakeSMSClient: Test Message: " + object_path.value());
  sms.SetString("Timestamp", "Fri Jun  8 13:26:04 EDT 2012");

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(sms)));
}

}  // namespace chromeos
