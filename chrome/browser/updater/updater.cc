// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/updater.h"

#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/updater/mojom/updater_service.mojom.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

namespace {

void GetUpdaterState(
    UpdaterScope scope,
    base::OnceCallback<void(const mojom::UpdaterState&)> callback) {
  BrowserUpdaterClient::Create(scope)->GetUpdaterState(std::move(callback));
}

void GetPoliciesJson(UpdaterScope scope,
                     base::OnceCallback<void(const std::string&)> callback) {
  BrowserUpdaterClient::Create(scope)->GetPoliciesJson(std::move(callback));
}

void GetAppStates(
    UpdaterScope scope,
    base::OnceCallback<void(const std::vector<mojom::AppState>&)> callback) {
  BrowserUpdaterClient::Create(scope)->GetAppStates(std::move(callback));
}

}  // namespace

std::optional<mojom::UpdateState> GetLastOnDemandUpdateState() {
  return GetLastOnDemandUpdateStateStorage();
}

std::optional<mojom::AppState> GetLastKnownBrowserRegistration() {
  return GetLastKnownBrowserRegistrationStorage();
}

std::optional<mojom::AppState> GetLastKnownUpdaterRegistration() {
  return GetLastKnownUpdaterRegistrationStorage();
}

#if !BUILDFLAG(IS_LINUX)
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
#endif  // BUILDFLAG(IS_LINUX)

void GetSystemUpdaterState(
    base::OnceCallback<void(const mojom::UpdaterState&)> callback) {
  GetUpdaterState(UpdaterScope::kSystem, std::move(callback));
}

void GetUserUpdaterState(
    base::OnceCallback<void(const mojom::UpdaterState&)> callback) {
  GetUpdaterState(UpdaterScope::kUser, std::move(callback));
}

void GetSystemPoliciesJson(
    base::OnceCallback<void(const std::string&)> callback) {
  GetPoliciesJson(UpdaterScope::kSystem, std::move(callback));
}

void GetUserPoliciesJson(
    base::OnceCallback<void(const std::string&)> callback) {
  GetPoliciesJson(UpdaterScope::kUser, std::move(callback));
}

void GetSystemUpdaterAppStates(
    base::OnceCallback<void(const std::vector<mojom::AppState>&)> callback) {
  GetAppStates(UpdaterScope::kSystem, std::move(callback));
}

void GetUserUpdaterAppStates(
    base::OnceCallback<void(const std::vector<mojom::AppState>&)> callback) {
  GetAppStates(UpdaterScope::kUser, std::move(callback));
}

}  // namespace updater
