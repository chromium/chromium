// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_NOTIFICATION_SERVICE_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"

class DefaultBrowserPromptNotificationService {
 public:
  DefaultBrowserPromptNotificationService(
      const DefaultBrowserPromptNotificationService&) = delete;
  DefaultBrowserPromptNotificationService& operator=(
      const DefaultBrowserPromptNotificationService&) = delete;
  DefaultBrowserPromptNotificationService();
  ~DefaultBrowserPromptNotificationService();

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnShowDefaultBrowserPromptChanged() = 0;
  };

  static DefaultBrowserPromptNotificationService* GetInstance();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetShowDefaultBrowserPrompt(bool show);
  bool get_show_default_browser_prompt() const {
    return show_default_browser_prompt_;
  }

 private:
  bool show_default_browser_prompt_ = false;

  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_STARTUP_DEFAULT_BROWSER_PROMPT_NOTIFICATION_SERVICE_H_
