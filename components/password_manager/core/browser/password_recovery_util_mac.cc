// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_recovery_util_mac.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

PasswordRecoveryUtilMac::PasswordRecoveryUtilMac(
    PrefService* local_state,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : local_state_(local_state),
      main_thread_task_runner_(main_thread_task_runner) {}

PasswordRecoveryUtilMac::~PasswordRecoveryUtilMac() {}

void PasswordRecoveryUtilMac::RecordPasswordRecovery() {
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](PrefService* local_state) {
                       local_state->SetTime(prefs::kPasswordRecovery,
                                            base::Time::Now());
                     },
                     local_state_));
}

}  // namespace password_manager
