// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/updater.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/updater/mojom/updater_service.mojom.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

std::optional<mojom::UpdateState> GetLastOnDemandUpdateState() {
  return GetLastOnDemandUpdateStateStorage();
}

std::optional<mojom::AppState> GetLastKnownBrowserRegistration() {
  return GetLastKnownBrowserRegistrationStorage();
}

std::optional<mojom::AppState> GetLastKnownUpdaterRegistration() {
  return GetLastKnownUpdaterRegistrationStorage();
}

void CheckForUpdate(
    base::RepeatingCallback<void(const UpdateService::UpdateState&)> callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(&GetBrowserUpdaterScope),
      base::BindOnce(
          [](base::RepeatingCallback<void(
                 const updater::UpdateService::UpdateState&)> callback,
             updater::UpdaterScope scope) {
            BrowserUpdaterClient::Create(scope)->CheckForUpdate(callback);
          },
          callback));
}

}  // namespace updater
