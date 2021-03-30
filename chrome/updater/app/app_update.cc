// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_update.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/version.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/setup.h"
#include "chrome/updater/updater_version.h"

namespace updater {

class AppUpdate : public App {
 private:
  ~AppUpdate() override = default;
  void Initialize() override;
  void Uninitialize() override;
  void FirstTaskRun() override;

  void SetupDone(int result);

  scoped_refptr<Configurator> config_;
};

void AppUpdate::Initialize() {
  config_ = base::MakeRefCounted<Configurator>(CreateGlobalPrefs());
}

void AppUpdate::Uninitialize() {
  PrefsCommitPendingWrites(config_->GetPrefService());
}

void AppUpdate::FirstTaskRun() {
  InstallCandidate(updater_scope(),
                   base::BindOnce(&AppUpdate::SetupDone, this));
}

void AppUpdate::SetupDone(int result) {
  Shutdown(result);
}

scoped_refptr<App> MakeAppUpdate() {
  return base::MakeRefCounted<AppUpdate>();
}

}  // namespace updater
