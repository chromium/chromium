// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/system_web_app_background_task.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace web_app {

SystemAppBackgroundTask::SystemAppBackgroundTask(
    Profile* profile,
    const SystemAppBackgroundTaskInfo& info)
    // TODO(https://crbug/1169745): Create the webcontents lazily.
    : web_contents_(content::WebContents::Create(
          content::WebContents::CreateParams(profile))),
      web_app_url_loader_(std::make_unique<WebAppUrlLoader>()),
      timer_(std::make_unique<base::RepeatingTimer>()),
      url_(info.url),
      period_(info.period),
      opened_count_(0),
      timer_activated_count_(0),
      open_immediately_(info.open_immediately) {}

SystemAppBackgroundTask::~SystemAppBackgroundTask() = default;

void SystemAppBackgroundTask::StartTask() {
  if (open_immediately_) {
    NavigateTimerBackgroundPage();
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
  timer_activated_count_++;
  auto prefs = web_contents_->GetOrCreateWebPreferences();

  prefs.allow_scripts_to_close_windows = true;
  web_contents_->SetWebPreferences(prefs);
  web_app_url_loader_->PrepareForLoad(
      web_contents_.get(),
      base::BindOnce(&SystemAppBackgroundTask::OnLoaderReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SystemAppBackgroundTask::OnLoaderReady(
    web_app::WebAppUrlLoader::Result result) {
  web_app_url_loader_->LoadUrl(
      url_, web_contents_.get(), WebAppUrlLoader::UrlComparison::kExact,
      base::BindOnce(&SystemAppBackgroundTask::OnPageReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SystemAppBackgroundTask::OnPageReady(
    web_app::WebAppUrlLoader::Result result) {
  if (result == WebAppUrlLoader::Result::kUrlLoaded)
    opened_count_++;
}

}  // namespace web_app
