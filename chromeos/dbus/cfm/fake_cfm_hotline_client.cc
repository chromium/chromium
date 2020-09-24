// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cfm/fake_cfm_hotline_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeCfmHotlineClient::FakeCfmHotlineClient() = default;
FakeCfmHotlineClient::~FakeCfmHotlineClient() = default;

void FakeCfmHotlineClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeCfmHotlineClient::BootstrapMojoConnection(
    base::ScopedFD fd,
    BootstrapMojoConnectionCallback result_callback) {
  // Fake that the mojo connection has been successfully established.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback), true));
}

void FakeCfmHotlineClient::AddObserver(cfm::CfmObserver* observer) {
  observer_list_.AddObserver(observer);
}

void FakeCfmHotlineClient::RemoveObserver(cfm::CfmObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

}  // namespace chromeos
