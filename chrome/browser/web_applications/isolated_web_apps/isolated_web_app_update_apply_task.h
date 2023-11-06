// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_APPLY_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_APPLY_TASK_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"

class ScopedKeepAlive;
class ScopedProfileKeepAlive;

namespace web_app {

struct IsolatedWebAppApplyUpdateCommandError;
class WebAppCommandScheduler;

// This task is responsible for applying a pending Isolated Web App update by
// calling `WebAppCommandScheduler::ApplyPendingIsolatedWebAppUpdate`.
class IsolatedWebAppUpdateApplyTask {
 public:
  using CompletionStatus =
      base::expected<void, IsolatedWebAppApplyUpdateCommandError>;
  using CompletionCallback = base::OnceCallback<void(CompletionStatus status)>;

  IsolatedWebAppUpdateApplyTask(
      IsolatedWebAppUrlInfo url_info,
      std::unique_ptr<ScopedKeepAlive> optional_keep_alive,
      std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
      WebAppCommandScheduler& command_scheduler);
  ~IsolatedWebAppUpdateApplyTask();

  IsolatedWebAppUpdateApplyTask(const IsolatedWebAppUpdateApplyTask&) = delete;
  IsolatedWebAppUpdateApplyTask& operator=(
      const IsolatedWebAppUpdateApplyTask&) = delete;

  void Start(CompletionCallback callback);

  bool has_started() const { return has_started_; }

  const IsolatedWebAppUrlInfo& url_info() const { return url_info_; }

  base::Value AsDebugValue() const;

 private:
  void OnUpdateApplied(CompletionStatus result);

  IsolatedWebAppUrlInfo url_info_;
  std::unique_ptr<ScopedKeepAlive> optional_keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive_;
  base::raw_ref<WebAppCommandScheduler> command_scheduler_;

  base::Value::Dict debug_log_;
  bool has_started_ = false;
  CompletionCallback callback_;

  base::WeakPtrFactory<IsolatedWebAppUpdateApplyTask> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_APPLY_TASK_H_
