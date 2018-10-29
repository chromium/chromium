// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_seneschal_client.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"

namespace chromeos {

FakeSeneschalClient::FakeSeneschalClient() {
  share_path_response_.set_success(true);
  share_path_response_.set_path("foo");
}

FakeSeneschalClient::~FakeSeneschalClient() = default;

void FakeSeneschalClient::SharePath(
    const vm_tools::seneschal::SharePathRequest& request,
    DBusMethodCallback<vm_tools::seneschal::SharePathResponse> callback) {
  share_path_called_ = true;
  last_request_ = request;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), share_path_response_));
}

}  // namespace chromeos
