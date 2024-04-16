// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/scheduler.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/cocoa/keystone_infobar_delegate.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

void DoPeriodicTasks(base::OnceClosure callback) {
  EnsureUpdater(base::BindOnce(&ShowUpdaterPromotionInfoBar),
                base::BindOnce(
                    // Run updater periodic tasks in case the launchd scheduled
                    // task is blocked.
                    [](base::OnceClosure callback) {
                      BrowserUpdaterClient::Create(::GetUpdaterScope())
                          ->RunPeriodicTasks(std::move(callback));
                    },
                    std::move(callback)));
}

}  // namespace updater
