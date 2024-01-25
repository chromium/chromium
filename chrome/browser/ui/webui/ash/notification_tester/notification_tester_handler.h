// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_NOTIFICATION_TESTER_NOTIFICATION_TESTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_NOTIFICATION_TESTER_NOTIFICATION_TESTER_HANDLER_H_

#include "content/public/browser/web_ui_message_handler.h"

namespace gfx {
class Image;
struct VectorIcon;
}  // namespace gfx

namespace message_center {
struct ButtonInfo;
class NotificationItem;
class RichNotificationData;
}  // namespace message_center

namespace ui {
class ImageModel;
}  // namespace ui

namespace ash {

// WebUI message handler for chrome://notification-tester from the front-end to
// the message center.
class NotificationTesterHandler : public content::WebUIMessageHandler {
 public:
  NotificationTesterHandler();
  NotificationTesterHandler(const NotificationTesterHandler&) = delete;
  NotificationTesterHandler& operator=(const NotificationTesterHandler&) =
      delete;
  ~NotificationTesterHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  // MessageHandler callback that fires when the MessageHandler receives a
  // message to generate a notification from the front-end.
  void HandleGenerateNotificationForm(const base::Value::List& args);

  // Given the name of an icon (string) within the notification tester
  // resources, return the corresponding icon.
  const ui::ImageModel GetNotificationIconFromString(
      const std::string& icon_name);

  // Given the name of an image (string) within the notification tester
  // resources, return the corresponding gfx::Image.
  const gfx::Image GetRichDataImageFromString(const std::string& image_name);

  // Given the name of a small image (string) such as 'kTerminalSshIcon',
  // return the corresponding gfx::VectorIcon.
  const gfx::VectorIcon& GetRichDataSmallImageFromString(
      const std::string& small_image_name);

  // Return a std::vector with 'num_buttons' ButtonInfo objects.
  std::vector<message_center::ButtonInfo> GetRichDataButtons(int num_buttons);

  // Return a std::vector with 'num_items' NotificationItem structs. Used for
  // NOTIFICATION_TYPE_MULTIPLE.
  std::vector<message_center::NotificationItem> GetRichDataNotifItems(
      int num_items);

  // Return a RichNotificationData object populated with the user-configured
  // notification object from the front-end.
  message_center::RichNotificationData DictToOptionalFields(
      const base::Value::Dict* notifObj);
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_NOTIFICATION_TESTER_NOTIFICATION_TESTER_HANDLER_H_
