// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPS_QUIT_WITH_APPS_CONTROLLER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_APPS_QUIT_WITH_APPS_CONTROLLER_MAC_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class PrefRegistrySimple;
class Profile;

namespace message_center {
class Notification;
}

// QuitWithAppsController checks whether any apps are running and shows a
// notification to quit all of them.
class QuitWithAppsController : public message_center::NotificationDelegate {
 public:
  static const char kQuitWithAppsNotificationID[];

  QuitWithAppsController();

  QuitWithAppsController(const QuitWithAppsController&) = delete;
  QuitWithAppsController& operator=(const QuitWithAppsController&) = delete;

  // NotificationDelegate interface.
  void Close(bool by_user) override;
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override;

  // Attempt to quit Chrome. This will display a notification and return false
  // if there are apps running.
  bool ShouldQuit();

  // Register prefs used by QuitWithAppsController.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  ~QuitWithAppsController() override;

  std::unique_ptr<message_center::Notification> notification_;
  // The Profile instance associated with the notification_. We need to cache
  // the instance here because when we want to cancel the notification we need
  // to provide the profile which was used to add the notification previously.
  // Not owned by this class.
  raw_ptr<Profile, DanglingUntriaged> notification_profile_ = nullptr;

  // Whether to suppress showing the notification for the rest of the session.
  bool suppress_for_session_ = false;
};

#endif  // CHROME_BROWSER_UI_COCOA_APPS_QUIT_WITH_APPS_CONTROLLER_MAC_H_
