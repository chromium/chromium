// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
#define CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/browser/status_icons/desktop_notification_balloon.h"
#include "chrome/browser/status_icons/status_icon.h"

@class MenuControllerCocoa;
@class NSStatusItem;
@class StatusItemController;

class StatusIconMac : public StatusIcon {
 public:
  StatusIconMac();

  StatusIconMac(const StatusIconMac&) = delete;
  StatusIconMac& operator=(const StatusIconMac&) = delete;

  ~StatusIconMac() override;

  // Overridden from StatusIcon.
  void SetImage(const gfx::ImageSkia& image) override;
  void SetToolTip(const std::u16string& tool_tip) override;
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override;

  bool HasStatusIconMenu();

 protected:
  // Overridden from StatusIcon.
  void UpdatePlatformContextMenu(StatusIconMenuModel* model) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(StatusIconMacTest, CreateMenu);
  FRIEND_TEST_ALL_PREFIXES(StatusIconMacTest, MenuToolTip);

  void SetToolTip(NSString* tool_tip);
  void CreateMenu(ui::MenuModel* model, NSString* tool_tip);

  // Getter for item_ that allows lazy initialization.
  NSStatusItem* item();
  NSStatusItem* __strong item_;

  StatusItemController* __strong controller_;

  // Notification balloon.
  DesktopNotificationBalloon notification_;

  NSString* __strong tool_tip_;

  // Status menu shown when right-clicking the system icon, if it has been
  // created by |UpdatePlatformContextMenu|.
  MenuControllerCocoa* __strong menu_;
};

#endif // CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
