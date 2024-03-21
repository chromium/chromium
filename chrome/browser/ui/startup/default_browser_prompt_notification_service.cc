// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt_notification_service.h"

#include "base/no_destructor.h"

DefaultBrowserPromptNotificationService::
    DefaultBrowserPromptNotificationService() = default;

DefaultBrowserPromptNotificationService::
    ~DefaultBrowserPromptNotificationService() = default;

// static
DefaultBrowserPromptNotificationService*
DefaultBrowserPromptNotificationService::GetInstance() {
  static base::NoDestructor<DefaultBrowserPromptNotificationService> instance;
  return instance.get();
}

void DefaultBrowserPromptNotificationService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}
void DefaultBrowserPromptNotificationService::RemoveObserver(
    Observer* observer) {
  observers_.RemoveObserver(observer);
}

void DefaultBrowserPromptNotificationService::SetShowDefaultBrowserPrompt(
    bool show) {
  if (show == show_default_browser_prompt_) {
    return;
  }
  show_default_browser_prompt_ = show;
  for (auto& obs : observers_) {
    obs.OnShowDefaultBrowserPromptChanged();
  }
}
