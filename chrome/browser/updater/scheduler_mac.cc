// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/scheduler.h"

#include "base/callback.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"

namespace updater {

void DoPeriodicTasks(base::OnceClosure callback) {
  BrowserUpdaterClient::Create(GetUpdaterScope())
      ->RunPeriodicTasks(std::move(callback));
}

}  // namespace updater
