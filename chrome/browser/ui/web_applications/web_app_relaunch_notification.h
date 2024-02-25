// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_RELAUNCH_NOTIFICATION_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_RELAUNCH_NOTIFICATION_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

enum class AppRelaunchState;

constexpr int kSecondsToShowNotificationPostAppRelaunch = 2;

void NotifyAppRelaunchState(const webapps::AppId& placeholder_app_id,
                            const webapps::AppId& final_app_id,
                            const std::u16string& final_app_name,
                            base::WeakPtr<Profile> profile,
                            AppRelaunchState relaunch_state);

scoped_refptr<base::SingleThreadTaskRunner>& GetTaskRunnerForTesting();

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_RELAUNCH_NOTIFICATION_H_
