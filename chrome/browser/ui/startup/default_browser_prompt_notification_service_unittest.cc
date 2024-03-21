// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/default_browser_prompt_notification_service.h"

#include "testing/gtest/include/gtest/gtest.h"

class DefaultBrowserPromptNotificationServiceTest
    : public testing::Test,
      public DefaultBrowserPromptNotificationService::Observer {
 public:
  void SetUp() override {
    auto* service = DefaultBrowserPromptNotificationService::GetInstance();
    service->SetShowDefaultBrowserPrompt(false);
    service->AddObserver(this);
    ResetHasPromptChanged();
  }

  void TearDown() override {
    auto* service = DefaultBrowserPromptNotificationService::GetInstance();
    service->RemoveObserver(this);
  }

  void OnShowDefaultBrowserPromptChanged() override {
    has_prompt_changed_ = true;
  }

  bool get_has_prompt_changed() const { return has_prompt_changed_; }

  void ResetHasPromptChanged() { has_prompt_changed_ = false; }

 private:
  bool has_prompt_changed_ = false;
};

TEST_F(DefaultBrowserPromptNotificationServiceTest, NotifiesObservers) {
  auto* service = DefaultBrowserPromptNotificationService::GetInstance();
  EXPECT_FALSE(get_has_prompt_changed());

  service->SetShowDefaultBrowserPrompt(true);
  EXPECT_TRUE(get_has_prompt_changed());
}

TEST_F(DefaultBrowserPromptNotificationServiceTest,
       DoesNotNotifyObserversWhenValueIsSame) {
  auto* service = DefaultBrowserPromptNotificationService::GetInstance();
  service->SetShowDefaultBrowserPrompt(true);
  EXPECT_TRUE(get_has_prompt_changed());

  ResetHasPromptChanged();
  service->SetShowDefaultBrowserPrompt(true);
  EXPECT_FALSE(get_has_prompt_changed());
}
