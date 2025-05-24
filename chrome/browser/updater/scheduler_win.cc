// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/scheduler.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/browser/updater/check_updater_health_task.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

void DoPeriodicTasks(base::OnceClosure callback) {
  base::MakeRefCounted<CheckUpdaterHealthTask>(GetBrowserUpdaterScope())
      ->Run(
          base::BindOnce(&BrowserUpdaterClient::RunPeriodicTasks,
                         BrowserUpdaterClient::Create(GetBrowserUpdaterScope()),
                         std::move(callback)));
}

}  // namespace updater
