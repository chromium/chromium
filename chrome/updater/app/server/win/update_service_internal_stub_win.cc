// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/server/win/update_service_internal_stub_win.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

namespace updater {

UpdateServiceInternalStubWin::UpdateServiceInternalStubWin(
    scoped_refptr<updater::UpdateServiceInternal> impl,
    base::RepeatingClosure task_start_listener,
    base::RepeatingClosure task_end_listener)
    : impl_(impl),
      task_start_listener_(task_start_listener),
      task_end_listener_(task_end_listener) {}

UpdateServiceInternalStubWin::~UpdateServiceInternalStubWin() = default;

void UpdateServiceInternalStubWin::Run(base::OnceClosure callback) {
  task_start_listener_.Run();
  impl_->Run(std::move(callback).Then(task_end_listener_));
}

void UpdateServiceInternalStubWin::Hello(base::OnceClosure callback) {
  task_start_listener_.Run();
  impl_->Hello(std::move(callback).Then(task_end_listener_));
}

}  // namespace updater
