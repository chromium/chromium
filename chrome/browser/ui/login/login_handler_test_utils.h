// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOGIN_LOGIN_HANDLER_TEST_UTILS_H_
#define CHROME_BROWSER_UI_LOGIN_LOGIN_HANDLER_TEST_UTILS_H_

#include <list>

#include "chrome/browser/auth_notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/test/test_utils.h"

class LoginHandler;

// Maintains a set of LoginHandlers that are currently active and
// keeps a count of the notifications that were observed.
class LoginPromptBrowserTestObserver : public content::NotificationObserver {
 public:
  LoginPromptBrowserTestObserver();

  LoginPromptBrowserTestObserver(const LoginPromptBrowserTestObserver&) =
      delete;
  LoginPromptBrowserTestObserver& operator=(
      const LoginPromptBrowserTestObserver&) = delete;

  ~LoginPromptBrowserTestObserver() override;

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  void AddHandler(LoginHandler* handler);

  void RemoveHandler(LoginHandler* handler);

  void Register(const content::NotificationSource& source);
  void UnregisterAll();

  const std::list<LoginHandler*>& handlers() const { return handlers_; }

  int auth_needed_count() const { return auth_needed_count_; }
  int auth_supplied_count() const { return auth_supplied_count_; }
  int auth_cancelled_count() const { return auth_cancelled_count_; }

 private:
  std::list<LoginHandler*> handlers_;

  // The exact number of notifications we receive is depedent on the
  // number of requests that were dispatched and is subject to a
  // number of factors that we don't directly control here.  The
  // values below should only be used qualitatively.
  int auth_needed_count_;
  int auth_supplied_count_;
  int auth_cancelled_count_;

 private:
  content::NotificationRegistrar registrar_;
};

template <int T>
class WindowedNavigationObserver
    : public content::WindowedNotificationObserver {
 public:
  explicit WindowedNavigationObserver(
      content::NavigationController* controller);

  WindowedNavigationObserver(const WindowedNavigationObserver&) = delete;
  WindowedNavigationObserver& operator=(const WindowedNavigationObserver&) =
      delete;
};

template <int T>
WindowedNavigationObserver<T>::WindowedNavigationObserver(
    content::NavigationController* controller)
    : content::WindowedNotificationObserver(
        T, content::Source<content::NavigationController>(controller)) {
}

typedef WindowedNavigationObserver<chrome::NOTIFICATION_AUTH_NEEDED>
    WindowedAuthNeededObserver;

typedef WindowedNavigationObserver<chrome::NOTIFICATION_AUTH_CANCELLED>
    WindowedAuthCancelledObserver;

typedef WindowedNavigationObserver<chrome::NOTIFICATION_AUTH_SUPPLIED>
    WindowedAuthSuppliedObserver;

// LOAD_STOP observer is special since we want to be able to wait for
// multiple LOAD_STOP events.
class WindowedLoadStopObserver
    : public WindowedNavigationObserver<content::NOTIFICATION_LOAD_STOP> {
 public:
  WindowedLoadStopObserver(content::NavigationController* controller,
                           int notification_count);

  WindowedLoadStopObserver(const WindowedLoadStopObserver&) = delete;
  WindowedLoadStopObserver& operator=(const WindowedLoadStopObserver&) = delete;

 protected:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  int remaining_notification_count_;  // Number of notifications remaining.
};

#endif  // CHROME_BROWSER_UI_LOGIN_LOGIN_HANDLER_TEST_UTILS_H_
