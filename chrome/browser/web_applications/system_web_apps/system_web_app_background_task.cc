// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_apps/system_web_app_background_task.h"
#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace web_app {

// Wait for 2 minutes before starting background tasks. User login is busy, and
// this will give a little time to settle down. We could get even more
// sophisticated, and smear all the different start_immediately tasks across a
// couple minutes instead of setting their start timers to the same time.
const int kInitialWaitForBackgroundTasksSeconds = 120;

SystemAppBackgroundTask::SystemAppBackgroundTask(
    Profile* profile,
    const SystemAppBackgroundTaskInfo& info)
    : profile_(profile),
      web_contents_(nullptr),
      web_app_url_loader_(std::make_unique<WebAppUrlLoader>()),
      timer_(std::make_unique<base::RepeatingTimer>()),
      start_immediately_timer_(std::make_unique<base::OneShotTimer>()),
      url_(info.url),
      period_(info.period),
      opened_count_(0),
      timer_activated_count_(0),
      open_immediately_(info.open_immediately) {}

SystemAppBackgroundTask::~SystemAppBackgroundTask() = default;

void SystemAppBackgroundTask::StartTask() {
  if (open_immediately_) {
    DCHECK_GT(period_, base::TimeDelta::FromSeconds(
                           kInitialWaitForBackgroundTasksSeconds));
    start_immediately_timer_->Start(
        FROM_HERE,
        base::TimeDelta::FromSeconds(kInitialWaitForBackgroundTasksSeconds),
        base::BindOnce(&SystemAppBackgroundTask::NavigateTimerBackgroundPage,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  timer_->Start(
      FROM_HERE, period_,
      base::BindRepeating(&SystemAppBackgroundTask::NavigateTimerBackgroundPage,
                          weak_ptr_factory_.GetWeakPtr()));
}

void SystemAppBackgroundTask::StopTask() {
  timer_.reset();
  web_contents_.reset();
}

void SystemAppBackgroundTask::NavigateTimerBackgroundPage() {
  if (!web_contents_) {
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
  }

  timer_activated_count_++;
  auto prefs = web_contents_->GetOrCreateWebPreferences();

  prefs.allow_scripts_to_close_windows = true;
  web_contents_->SetWebPreferences(prefs);
  web_app_url_loader_->PrepareForLoad(
      web_contents_.get(),
      base::BindOnce(&SystemAppBackgroundTask::OnLoaderReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SystemAppBackgroundTask::OnLoaderReady(WebAppUrlLoader::Result result) {
  web_app_url_loader_->LoadUrl(
      url_, web_contents_.get(), WebAppUrlLoader::UrlComparison::kExact,
      base::BindOnce(&SystemAppBackgroundTask::OnPageReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SystemAppBackgroundTask::OnPageReady(WebAppUrlLoader::Result result) {
  if (result == WebAppUrlLoader::Result::kUrlLoaded)
    opened_count_++;
}

}  // namespace web_app
