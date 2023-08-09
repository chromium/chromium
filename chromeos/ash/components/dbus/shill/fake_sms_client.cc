// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/fake_sms_client.h"

#include <string>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chromeos/dbus/constants/dbus_switches.h"

namespace ash {

// static
const char FakeSMSClient::kNumber[] = "000-000-0000";
const char FakeSMSClient::kTimestamp[] = "Fri Jun  8 13:26:04 EDT 2012";

FakeSMSClient::FakeSMSClient() = default;

FakeSMSClient::~FakeSMSClient() = default;

void FakeSMSClient::GetAll(const std::string& service_name,
                           const dbus::ObjectPath& object_path,
                           GetAllCallback callback) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kSmsTestMessages)) {
    return;
  }

  pending_get_all_callback_ = std::move(callback);
  pending_get_all_object_path_ = object_path;
}

void FakeSMSClient::CompleteGetAll() {
  DCHECK(pending_get_all_callback_) << "No pending call to GetAll()";

  base::Value::Dict sms;
  sms.Set("Number", kNumber);
  sms.Set("Text", "FakeSMSClient: Test Message: " +
                      pending_get_all_object_path_.value());
  sms.Set("Timestamp", kTimestamp);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(pending_get_all_callback_), std::move(sms)));
}

}  // namespace ash
