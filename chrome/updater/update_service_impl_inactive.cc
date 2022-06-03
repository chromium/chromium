// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_impl_inactive.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/version.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/update_service.h"

namespace updater {

namespace {

class UpdateServiceImplInactive : public UpdateService {
 public:
  UpdateServiceImplInactive() = default;

  // Overrides for updater::UpdateService.
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) const override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::Version()));
  }

  void RegisterApp(
      const RegistrationRequest& request,
      base::OnceCallback<void(const RegistrationResponse&)> callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), RegistrationResponse(-1)));
  }

  void RunPeriodicTasks(base::OnceClosure callback) override {
    std::move(callback).Run();
  }

  void UpdateAll(StateChangeCallback state_update, Callback callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UpdateService::Result::kInactive));
  }

  void Update(const std::string& app_id,
              Priority priority,
              StateChangeCallback state_update,
              Callback callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UpdateService::Result::kInactive));
  }

  void Uninitialize() override {}

 private:
  ~UpdateServiceImplInactive() override = default;
};

}  // namespace

scoped_refptr<UpdateService> MakeInactiveUpdateService() {
  return base::MakeRefCounted<UpdateServiceImplInactive>();
}

}  // namespace updater
