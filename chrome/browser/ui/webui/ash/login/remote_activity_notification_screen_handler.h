// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_REMOTE_ACTIVITY_NOTIFICATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_REMOTE_ACTIVITY_NOTIFICATION_SCREEN_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/base_screen_handler.h"
#include "components/login/localized_values_builder.h"

namespace ash {

class RemoteActivityNotificationView {
 public:
  inline constexpr static StaticOobeScreenId kScreenId{
      "remote-activity-notification", "RemoteActivityNotificationScreen"};

  virtual ~RemoteActivityNotificationView() = default;

  virtual void Show() = 0;
  virtual base::WeakPtr<RemoteActivityNotificationView> AsWeakPtr() = 0;
};

class RemoteActivityNotificationScreenHandler final
    : public RemoteActivityNotificationView,
      public BaseScreenHandler {
 public:
  using TView = RemoteActivityNotificationView;

  RemoteActivityNotificationScreenHandler();
  ~RemoteActivityNotificationScreenHandler() override;
  RemoteActivityNotificationScreenHandler(
      const RemoteActivityNotificationScreenHandler&) = delete;
  RemoteActivityNotificationScreenHandler& operator=(
      const RemoteActivityNotificationScreenHandler&) = delete;

 private:
  // implements `RemoteActivityNotificationView`:
  void Show() override;
  base::WeakPtr<RemoteActivityNotificationView> AsWeakPtr() override;

  // `BaseScreenHandler` implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;

  base::WeakPtrFactory<RemoteActivityNotificationView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOGIN_REMOTE_ACTIVITY_NOTIFICATION_SCREEN_HANDLER_H_
