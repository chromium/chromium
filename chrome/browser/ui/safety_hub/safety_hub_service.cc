// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/safety_hub_service.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"

SafetyHubService::Result::Result(base::TimeTicks timestamp)
    : timestamp_(timestamp) {}

base::TimeTicks SafetyHubService::Result::timestamp() const {
  return timestamp_;
}

SafetyHubService::SafetyHubService() = default;
SafetyHubService::~SafetyHubService() = default;

void SafetyHubService::Shutdown() {
  update_timer_.Stop();
}

void SafetyHubService::StartRepeatedUpdates() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  UpdateAsync();
  update_timer_.Start(FROM_HERE, GetRepeatedUpdateInterval(),
                      base::BindRepeating(&SafetyHubService::UpdateAsync,
                                          base::Unretained(this)));
}

void SafetyHubService::UpdateAsync() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (pending_updates_++) {
    return;
  }
  UpdateAsyncInternal();
}

void SafetyHubService::UpdateAsyncInternal() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT}, GetBackgroundTask(),
      base::BindOnce(&SafetyHubService::OnUpdateFinished, GetAsWeakRef()));
}

void SafetyHubService::OnUpdateFinished(
    std::unique_ptr<SafetyHubService::Result> result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  latest_result_ = UpdateOnUIThread(std::move(result));
  NotifyObservers(latest_result_.get());
  if (--pending_updates_) {
    UpdateAsyncInternal();
  }
}

void SafetyHubService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SafetyHubService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SafetyHubService::NotifyObservers(Result* result) {
  for (auto& observer : observers_) {
    observer.OnResultAvailable(result);
  }
}

bool SafetyHubService::IsUpdateRunning() {
  return pending_updates_ > 0;
}

absl::optional<SafetyHubService::Result*> SafetyHubService::GetCachedResult() {
  if (latest_result_ != nullptr) {
    return latest_result_.get();
  }
  return absl::nullopt;
}
