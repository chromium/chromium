// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_wake.h"

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service_internal.h"

namespace updater {

// AppWake is a simple client which dials the same-versioned server via RPC.
// This is done via the UpdateServiceInternal interface.
class AppWake : public App {
 public:
  AppWake() = default;

 private:
  ~AppWake() override = default;

  // Overrides for App.
  void FirstTaskRun() override;
};

void AppWake::FirstTaskRun() {
  CreateUpdateServiceInternalProxy(updater_scope())
      ->Run(base::BindOnce(&AppWake::Shutdown, this, kErrorOk));
}

scoped_refptr<App> MakeAppWake() {
  return base::MakeRefCounted<AppWake>();
}

}  // namespace updater
