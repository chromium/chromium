// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_MESSAGE_CONTROLLER_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_MESSAGE_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/messages/android/message_enums.h"
#include "components/messages/android/message_wrapper.h"
#include "components/messages/android/throttler/domain_session_throttler.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class WebContents;
}  // namespace content

namespace webapps {

class InstallableAmbientBadgeClient;

// Message controller for a message shown to users when they visit a
// progressive web app. Tapping primary button triggers the add to home screen
// flow.
class InstallableAmbientBadgeMessageController {
 public:
  explicit InstallableAmbientBadgeMessageController(
      InstallableAmbientBadgeClient* client);
  ~InstallableAmbientBadgeMessageController();

  // Returns true if the message was enqueued with EnqueueMessage() method, but
  // wasn't dismissed yet.
  bool IsMessageEnqueued();

  // Enqueues a message to be displayed on the screen. Typically there are no
  // other messages on the screen and enqueued message will get displayed
  // immediately.
  void EnqueueMessage(content::WebContents* web_contents,
                      const std::u16string& app_name,
                      const SkBitmap& icon,
                      const bool is_primary_icon_maskable,
                      const GURL& start_url);

  // Dismisses displayed message. This method is safe to call  when there is no
  // displayed message.
  void DismissMessage();

 private:
  static messages::DomainSessionThrottler* GetThrottler();

  void HandleInstallButtonClicked();
  void HandleMessageDismissed(messages::DismissReason dismiss_reason);

  raw_ptr<InstallableAmbientBadgeClient> client_;
  std::unique_ptr<messages::MessageWrapper> message_;
  url::Origin save_origin_;

  base::WeakPtrFactory<InstallableAmbientBadgeMessageController> weak_factory_{
      this};
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_MESSAGE_CONTROLLER_H_
