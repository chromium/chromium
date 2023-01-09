// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_internal_impl_inactive.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

namespace {

class UpdateServiceInternalImplInactive : public UpdateServiceInternal {
 public:
  UpdateServiceInternalImplInactive() = default;

  // Overrides for updater::UpdateServiceInternal.
  void Run(base::OnceClosure callback) override {
    VLOG(1) << __func__ << " (Inactive)";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }

  void Hello(base::OnceClosure callback) override {
    VLOG(1) << __func__ << " (Inactive)";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }

 private:
  ~UpdateServiceInternalImplInactive() override = default;
};

}  // namespace

scoped_refptr<UpdateServiceInternal> MakeInactiveUpdateServiceInternal() {
  return base::MakeRefCounted<UpdateServiceInternalImplInactive>();
}

}  // namespace updater
