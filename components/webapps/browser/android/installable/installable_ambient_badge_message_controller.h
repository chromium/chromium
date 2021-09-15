// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_MESSAGE_CONTROLLER_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_MESSAGE_CONTROLLER_H_

namespace webapps {

// Message controller for a message shown to users when they visit a
// progressive web app. Tapping primary button triggers the add to home screen
// flow.
class InstallableAmbientBadgeMessageController {
 public:
  // Returns true if the message was enqueued with EnqueueMessage() method, but
  // wasn't dismissed yet.
  bool IsMessageEnqueued();

  // Enqueues a message to be displayed on the screen. Typically there are no
  // other messages on the screen and enqueued message will get displayed
  // immediately.
  void EnqueueMessage();

  // Dismisses displayed message. This method is safe to call  when there is no
  // displayed message.
  void DismissMessage();

 private:
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_MESSAGE_CONTROLLER_H_
