// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/cdm_factory_daemon_proxy.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"

namespace chromeos {

CdmFactoryDaemonProxy::CdmFactoryDaemonProxy()
    : mojo_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {}

CdmFactoryDaemonProxy::~CdmFactoryDaemonProxy() = default;

void CdmFactoryDaemonProxy::BindReceiver(
    mojo::PendingReceiver<BrowserCdmFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace chromeos
