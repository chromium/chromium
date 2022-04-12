// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/system_web_apps/system_web_app_background_task.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/base/idle/idle.h"

namespace web_app {

SystemAppBackgroundTaskInfo::~SystemAppBackgroundTaskInfo() = default;
SystemAppBackgroundTaskInfo::SystemAppBackgroundTaskInfo() = default;

SystemAppBackgroundTaskInfo::SystemAppBackgroundTaskInfo(
    const SystemAppBackgroundTaskInfo& other) = default;

SystemAppBackgroundTaskInfo::SystemAppBackgroundTaskInfo(
    const absl::optional<base::TimeDelta>& period,
    const GURL& url,
    bool open_immediately)
    : period(period), url(url), open_immediately(open_immediately) {}

SystemAppBackgroundTask::SystemAppBackgroundTask(
    Profile* profile,
    const SystemAppBackgroundTaskInfo& info)
    : profile_(profile),
      web_contents_(nullptr),
      web_app_url_loader_(std::make_unique<WebAppUrlLoader>()),
      timer_(std::make_unique<base::OneShotTimer>()),
      url_(info.url),
      period_(info.period),
      open_immediately_(info.open_immediately),
      delegate_(this) {}

SystemAppBackgroundTask::~SystemAppBackgroundTask() = default;

void SystemAppBackgroundTask::StartTask() {
  if (open_immediately_ ||
      period_ < base::Seconds(kInitialWaitForBackgroundTasksSeconds)) {
    timer_->Start(FROM_HERE,
                  base::Seconds(kInitialWaitForBackgroundTasksSeconds),
                  base::BindOnce(&SystemAppBackgroundTask::MaybeOpenPage,
                                 weak_ptr_factory_.GetWeakPtr()));
    state_ = INITIAL_WAIT;
  } else if (period_) {
    timer_->Start(FROM_HERE, period_.value(),
                  base::BindOnce(&SystemAppBackgroundTask::MaybeOpenPage,
                                 weak_ptr_factory_.GetWeakPtr()));
    state_ = WAIT_PERIOD;
  }
}

void SystemAppBackgroundTask::StopTask() {
  timer_.reset();
  web_contents_.reset();
}

void SystemAppBackgroundTask::MaybeOpenPage() {
  ui::IdleState idle_state = ui::CalculateIdleState(kIdleThresholdSeconds);
  base::Time now = base::Time::Now();
  // Start polling
  if (polling_since_time_.is_null()) {
    polling_since_time_ = now;
  }

  base::TimeDelta polling_duration = (now - polling_since_time_);

  if (polling_duration < base::Seconds(kIdlePollMaxTimeToWaitSeconds) &&
      idle_state == ui::IDLE_STATE_ACTIVE) {
    // We've gone through some weird clock adjustment (daylight savings?) that's
    // sent us back in time. We don't know what's going on, so zero the polling
    // time and stop polling.
    if (polling_duration < base::Seconds(0) && period_) {
      timer_->Start(FROM_HERE, period_.value(),
                    base::BindOnce(&SystemAppBackgroundTask::MaybeOpenPage,
                                   weak_ptr_factory_.GetWeakPtr()));
      state_ = WAIT_PERIOD;
      polling_since_time_ = base::Time();
      return;
    }
    // Poll
    timer_->Start(FROM_HERE, base::Seconds(kIdlePollIntervalSeconds),
                  base::BindOnce(&SystemAppBackgroundTask::MaybeOpenPage,
                                 weak_ptr_factory_.GetWeakPtr()));
    state_ = WAIT_IDLE;
    return;
  }

  if (period_) {
    timer_->Start(FROM_HERE, period_.value(),
                  base::BindOnce(&SystemAppBackgroundTask::MaybeOpenPage,
                                 weak_ptr_factory_.GetWeakPtr()));
  }

  polling_since_time_ = base::Time();
  state_ = WAIT_PERIOD;
  NavigateBackgroundPage();
}

void SystemAppBackgroundTask::CloseDelegate::CloseContents(
    content::WebContents* contents) {
  task_->CloseWebContents(contents);
}

void SystemAppBackgroundTask::CloseWebContents(content::WebContents* contents) {
  DCHECK(contents == web_contents_.get());
  web_contents_.reset();
}

void SystemAppBackgroundTask::NavigateBackgroundPage() {
  if (!web_contents_) {
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_contents_->SetDelegate(&delegate_);
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
  if (web_contents_) {
    web_app_url_loader_->LoadUrl(
        url_, web_contents_.get(), WebAppUrlLoader::UrlComparison::kExact,
        base::BindOnce(&SystemAppBackgroundTask::OnPageReady,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void SystemAppBackgroundTask::OnPageReady(WebAppUrlLoader::Result result) {
  if (result == WebAppUrlLoader::Result::kUrlLoaded)
    opened_count_++;
}

}  // namespace web_app
