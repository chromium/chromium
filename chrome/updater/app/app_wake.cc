// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_wake.h"

#include "base/bind.h"
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

  scoped_refptr<UpdateServiceInternal> update_service_internal_;
};

void AppWake::FirstTaskRun() {
  // The service creation might need task runners and the update service
  // internal needs to be instantiated after the base class has initialized
  // the thread pool.
  //
  // TODO(crbug.com/1113448) - consider initializing the thread pool in the
  // constructor of the base class or earlier, in the updater main.
  update_service_internal_ = CreateUpdateServiceInternalProxy(updater_scope());
  update_service_internal_->Run(
      base::BindOnce(&AppWake::Shutdown, this, kErrorOk));
}

scoped_refptr<App> MakeAppWake() {
  return base::MakeRefCounted<AppWake>();
}

}  // namespace updater
