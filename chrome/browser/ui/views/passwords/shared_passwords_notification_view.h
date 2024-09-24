// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_SHARED_PASSWORDS_NOTIFICATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_SHARED_PASSWORDS_NOTIFICATION_VIEW_H_

#include "chrome/browser/ui/passwords/bubble_controllers/password_bubble_controller_base.h"
#include "chrome/browser/ui/passwords/bubble_controllers/shared_passwords_notifications_bubble_controller.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_base.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"

// Bubble notifying the user that some of the stored credentials for this
// website have been received via the password sharing feature from other users.
class SharedPasswordsNotificationView : public PasswordBubbleViewBase {
  METADATA_HEADER(SharedPasswordsNotificationView, PasswordBubbleViewBase)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTopView);
  explicit SharedPasswordsNotificationView(content::WebContents* web_contents,
                                           views::View* anchor_view);
  ~SharedPasswordsNotificationView() override;

 private:
  // PasswordBubbleViewBase
  SharedPasswordsNotificationBubbleController* GetController() override;
  const SharedPasswordsNotificationBubbleController* GetController()
      const override;
  ui::ImageModel GetWindowIcon() override;

  SharedPasswordsNotificationBubbleController controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_SHARED_PASSWORDS_NOTIFICATION_VIEW_H_
