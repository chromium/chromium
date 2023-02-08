// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_update.h"

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/setup.h"

namespace updater {

class AppUpdate : public App {
 private:
  ~AppUpdate() override = default;
  void Initialize() override;
  void Uninitialize() override;
  void FirstTaskRun() override;

  void SetupDone(int result);
};

void AppUpdate::Initialize() {}

void AppUpdate::Uninitialize() {}

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
