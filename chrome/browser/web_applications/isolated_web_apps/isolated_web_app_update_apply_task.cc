// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_apply_task.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"

namespace web_app {

IsolatedWebAppUpdateApplyTask::IsolatedWebAppUpdateApplyTask(
    IsolatedWebAppUrlInfo url_info,
    std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    WebAppCommandScheduler& command_scheduler)
    : url_info_(std::move(url_info)),
      optional_keep_alive_(std::move(optional_keep_alive)),
      optional_profile_keep_alive_(std::move(optional_profile_keep_alive)),
      command_scheduler_(command_scheduler) {
  debug_log_ = base::Value::Dict()
                   .Set("bundle_id", url_info_.web_bundle_id().id())
                   .Set("app_id", url_info_.app_id());
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

void IsolatedWebAppUpdateApplyTask::OnUpdateApplied(CompletionStatus result) {
  debug_log_.Set("end_time", base::TimeToValue(base::Time::Now()));
  debug_log_.Set("result", result
                               .transform_error([](const auto& error) {
                                 return "Error: " + error.message;
                               })
                               .error_or("Success"));

  std::move(callback_).Run(std::move(result));
}

}  // namespace web_app
