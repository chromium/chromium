// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_apply_task.h"

#include <memory>
#include <optional>
#include <variant>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolation_data.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/isolated_web_apps/commands/copy_bundle_to_cache_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "components/webapps/isolated_web_apps/error/uma_logging.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kCopyToCacheResult[] = "copy_to_cache_result";
constexpr char kCopyBundleToCacheAfterUpdateMetric[] =
    "WebApp.Isolated.CopyBundleToCacheAfterUpdate";
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

IsolatedWebAppUpdateApplyTask::IsolatedWebAppUpdateApplyTask(
    IsolatedWebAppUrlInfo url_info,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    WebAppCommandScheduler& command_scheduler,
    Profile* profile)
    : url_info_(std::move(url_info)),
      optional_keep_alive_(std::move(optional_keep_alive)),
      optional_profile_keep_alive_(std::move(optional_profile_keep_alive)),
      command_scheduler_(command_scheduler),
      profile_(CHECK_DEREF(profile)) {
  debug_log_ = base::Value::Dict()
                   .Set("bundle_id", url_info_.web_bundle_id().id())
                   .Set("app_id", url_info_.app_id());
#if BUILDFLAG(IS_CHROMEOS)
  if (IsIwaBundleCacheEnabledInCurrentSession()) {
    debug_log_.Set("bundle_cache", "IWA bundle cache is enabled");
    cache_client_ = std::make_unique<IwaCacheClient>();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

IsolatedWebAppUpdateApplyTask::~IsolatedWebAppUpdateApplyTask() = default;

void IsolatedWebAppUpdateApplyTask::Start(CompletionCallback callback) {
  CHECK(!has_started_);
  has_started_ = true;
  callback_ = std::move(callback);

  debug_log_.Set("start_time", base::TimeToValue(base::Time::Now()));

  command_scheduler_->ApplyPendingIsolatedWebAppUpdate(
      url_info_, std::move(optional_keep_alive_),
      std::move(optional_profile_keep_alive_),
      base::BindOnce(&IsolatedWebAppUpdateApplyTask::OnUpdateApplied,
                     weak_factory_.GetWeakPtr()));
}

base::Value IsolatedWebAppUpdateApplyTask::AsDebugValue() const {
  return base::Value(debug_log_.Clone());
}

void IsolatedWebAppUpdateApplyTask::OnUpdateApplied(
    IsolatedWebAppApplyUpdateCommandResult result) {
  debug_log_.Set("end_time", base::TimeToValue(base::Time::Now()));
  debug_log_.Set("result", result
                               .transform_error([](const auto& error) {
                                 return "Error: " + error.message;
                               })
                               .error_or("Success"));

#if BUILDFLAG(IS_CHROMEOS)
  if (result.has_value() && IsIwaBundleCacheEnabledInCurrentSession()) {
    CopyUpdatedBundleToCache();
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  std::move(callback_).Run(std::move(result));
}

#if BUILDFLAG(IS_CHROMEOS)
void IsolatedWebAppUpdateApplyTask::CopyUpdatedBundleToCache() {
  command_scheduler_->CopyIsolatedWebAppBundleToCache(
      url_info_, IwaCacheClient::GetCurrentSessionType(),
      base::BindOnce(&IsolatedWebAppUpdateApplyTask::OnBundleCopiedToCache,
                     weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppUpdateApplyTask::OnBundleCopiedToCache(
    CopyBundleToCacheResult result) {
  web_app::UmaLogExpectedStatus(kCopyBundleToCacheAfterUpdateMetric, result);
  if (result.has_value()) {
    debug_log_.Set(kCopyToCacheResult,
                   "Successfully copied bundle to: " +
                       result->cached_bundle_path().MaybeAsASCII());
    std::move(callback_).Run(base::ok());
    return;
  }
  debug_log_.Set(kCopyToCacheResult,
                 CopyBundleToCacheErrorToString(result.error()));
  std::move(callback_).Run(
      base::unexpected<IsolatedWebAppApplyUpdateCommandError>(
          IsolatedWebAppApplyUpdateCommandError{
              .message = kCopyToCacheFailedMessage}));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace web_app
