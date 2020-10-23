// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/control_service_impl_inactive.h"

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/updater/control_service.h"

namespace updater {

namespace {

class ControlServiceImplInactive : public ControlService {
 public:
  ControlServiceImplInactive() = default;

  // Overrides for updater::ControlService.
  void Run(base::OnceClosure callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(callback));
  }

  void InitializeUpdateService(base::OnceClosure callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     std::move(callback));
  }

  void Uninitialize() override {}

 private:
  ~ControlServiceImplInactive() override = default;
};

}  // namespace

scoped_refptr<ControlService> MakeInactiveControlService() {
  return base::MakeRefCounted<ControlServiceImplInactive>();
}

}  // namespace updater
