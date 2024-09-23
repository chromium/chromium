// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/scheduler.h"

#include "base/functional/callback.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

void DoPeriodicTasks(base::OnceClosure callback) {
  BrowserUpdaterClient::Create(::GetUpdaterScope())
      ->RunPeriodicTasks(std::move(callback));
}

}  // namespace updater
