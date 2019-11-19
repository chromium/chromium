// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/login_handler_test_utils.h"

#include "chrome/browser/ui/login/login_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

LoginPromptBrowserTestObserver::LoginPromptBrowserTestObserver()
    : auth_needed_count_(0), auth_supplied_count_(0), auth_cancelled_count_(0) {
}

LoginPromptBrowserTestObserver::~LoginPromptBrowserTestObserver() {
}

void LoginPromptBrowserTestObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_AUTH_NEEDED) {
    AddHandler(content::Details<LoginNotificationDetails>(details)->handler());
    auth_needed_count_++;
  } else if (type == chrome::NOTIFICATION_AUTH_SUPPLIED) {
    RemoveHandler(content::Details<AuthSuppliedLoginNotificationDetails>(
                      details)->handler());
    auth_supplied_count_++;
  } else if (type == chrome::NOTIFICATION_AUTH_CANCELLED) {
    RemoveHandler(
        content::Details<LoginNotificationDetails>(details)->handler());
    auth_cancelled_count_++;
  }
}

void LoginPromptBrowserTestObserver::AddHandler(LoginHandler* handler) {
  ASSERT_FALSE(base::Contains(handlers_, handler));
  handlers_.push_back(handler);
}

void LoginPromptBrowserTestObserver::RemoveHandler(LoginHandler* handler) {
  auto i = std::find(handlers_.begin(), handlers_.end(), handler);
  // Cannot use ASSERT_NE, because gTest on Android confuses iterators with
  // containers.
  //
  // TODO(davidben): NOTIFICATION_AUTH_SUPPLIED and NOTIFICATION_AUTH_CANCELLED
  // are not quite guaranteed to come after NOTIFICATION_AUTH_NEEDED. Either
  // remove this assumption from the test class or fix things so this assumption
  // holds.
  ASSERT_TRUE(i != handlers_.end());
  handlers_.erase(i);
}

void LoginPromptBrowserTestObserver::Register(
    const content::NotificationSource& source) {
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_NEEDED, source);
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_SUPPLIED, source);
  registrar_.Add(this, chrome::NOTIFICATION_AUTH_CANCELLED, source);
}

void LoginPromptBrowserTestObserver::UnregisterAll() {
  registrar_.RemoveAll();
}

WindowedLoadStopObserver::WindowedLoadStopObserver(
    content::NavigationController* controller,
    int notification_count)
    : WindowedNavigationObserver<content::NOTIFICATION_LOAD_STOP>(controller),
      remaining_notification_count_(notification_count) {
  // This should really be an ASSERT, if those were allowed in a method which
  // does not return void.
  EXPECT_LE(0, remaining_notification_count_);
}

void WindowedLoadStopObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  ASSERT_LT(0, remaining_notification_count_);
  if (--remaining_notification_count_ == 0)
    WindowedNotificationObserver::Observe(type, source, details);
}
