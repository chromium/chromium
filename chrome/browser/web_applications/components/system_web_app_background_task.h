// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_SYSTEM_WEB_APP_BACKGROUND_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_SYSTEM_WEB_APP_BACKGROUND_TASK_H_

#include <memory.h>

#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/web_applications/components/system_web_app_types.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents.h"

class Profile;

namespace web_app {

// A struct used to configure a periodic background task for a SWA.
struct SystemAppBackgroundTaskInfo {
  SystemAppBackgroundTaskInfo() = default;
  SystemAppBackgroundTaskInfo(const base::TimeDelta& period,
                              const GURL& url,
                              bool open_immediately = false)
      : period(period), url(url), open_immediately(open_immediately) {}

  // The amount of time between each opening of the background url.
  // The url is opened using the same WebContents, so if the
  // previous task is still running, it will be closed.
  base::TimeDelta period;

  // The url of the background page to open. This should do one specific thing.
  // (Probably opening a shared worker, waiting for a response, and closing)
  GURL url;

  // A flag to indicate that the task should be opened immediately upon user
  // login, after the SWAs are done installing as opposed to waiting for the
  // first period to be reached.
  bool open_immediately;
};

// Used to manage a running periodic background task for a SWA.
class SystemAppBackgroundTask {
 public:
  SystemAppBackgroundTask(Profile* profile,
                          const SystemAppBackgroundTaskInfo& info);
  ~SystemAppBackgroundTask();

  // Start the timer, at the specified period. This will also run immediately if
  // needed
  void StartTask();

  // Bring down the background task if open, and stop the timer.
  void StopTask();

  bool open_immediately_for_testing() const { return open_immediately_; }

  SystemAppType app_type_for_testing() const { return app_type_; }

  GURL url_for_testing() const { return url_; }

  base::TimeDelta period_for_testing() const { return period_; }

  unsigned long opened_count_for_testing() const { return opened_count_; }

  unsigned long timer_activated_count_for_testing() const {
    return timer_activated_count_;
  }

  WebAppUrlLoader* UrlLoaderForTesting() { return web_app_url_loader_.get(); }

  // Set the url loader for testing. Takes ownership of the argument.
  void SetUrlLoaderForTesting(std::unique_ptr<WebAppUrlLoader> loader) {
    web_app_url_loader_.reset(loader.release());
  }

 private:
  void NavigateTimerBackgroundPage();
  void OnLoaderReady(web_app::WebAppUrlLoader::Result);
  void OnPageReady(web_app::WebAppUrlLoader::Result);

  SystemAppType app_type_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<WebAppUrlLoader> web_app_url_loader_;
  std::unique_ptr<base::RepeatingTimer> timer_;
  GURL url_;
  base::TimeDelta period_;
  unsigned long opened_count_;
  unsigned long timer_activated_count_;
  bool open_immediately_;

  base::WeakPtrFactory<SystemAppBackgroundTask> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_SYSTEM_WEB_APP_BACKGROUND_TASK_H_
