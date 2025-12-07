// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_display_host.h"

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"

namespace ash {

// static
LoginDisplayHost* LoginDisplayHost::default_host_ = nullptr;

LoginDisplayHost::LoginDisplayHost() {
  DCHECK(default_host() == nullptr);
  default_host_ = this;
}

LoginDisplayHost::~LoginDisplayHost() {
  for (auto& callback : completion_callbacks_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }

  default_host_ = nullptr;
}

void LoginDisplayHost::Finalize(base::OnceClosure completion_callback) {
  if (completion_callback.is_null()) {
    return;
  }

  completion_callbacks_.push_back(std::move(completion_callback));
}

void LoginDisplayHost::StartUserAdding(base::OnceClosure completion_callback) {
  if (completion_callback.is_null()) {
    return;
  }

  completion_callbacks_.push_back(std::move(completion_callback));
}

void LoginDisplayHost::AddWizardCreatedObserverForTests(
    base::RepeatingClosure on_created) {
  DCHECK(!on_wizard_controller_created_for_tests_);
  on_wizard_controller_created_for_tests_ = std::move(on_created);
}

void LoginDisplayHost::NotifyWizardCreated() {
  if (on_wizard_controller_created_for_tests_) {
    on_wizard_controller_created_for_tests_.Run();
  }
}

}  // namespace ash
