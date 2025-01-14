// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
#define CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"
#include "chrome/browser/status_icons/desktop_notification_balloon.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "chrome/browser/status_icons/status_tray.h"

@class MenuControllerCocoa;
@class NSStatusItem;
@class StatusItemController;

class StatusIconMac : public StatusIcon, public StatusIconMenuModel::Observer {
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
  void SetOpenMenuWithSecondaryClick(
      bool open_menu_with_secondary_click) override;
  void SetImageTemplate(bool is_template) override;

  // StatusIconMenuModel::Observer overrides:
  void OnMenuStateChanged() override;

  bool HasStatusIconMenu();

  // When open_menu_with_secondary_click_ is true, do not set the status item's
  // menu, so that left click will not open a menu. When a secondary click is
  // detected, this function is called which creates and sets the menu on the
  // status item, simulates a click, and then unsets the menu afterwards.
  void CreateAndOpenMenu();

  bool open_menu_with_secondary_click() {
    return open_menu_with_secondary_click_;
  }

 protected:
  // Overridden from StatusIcon.
  void UpdatePlatformContextMenu(StatusIconMenuModel* model) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(StatusIconMacTest, CreateMenu);
  FRIEND_TEST_ALL_PREFIXES(StatusIconMacTest, MenuToolTip);
  FRIEND_TEST_ALL_PREFIXES(StatusIconMacTest, SecondaryClickMenuNoToolTip);

  void CreateMenu(ui::MenuModel* model);

  // Getter for item_ that allows lazy initialization.
  NSStatusItem* item();
  NSStatusItem* __strong item_;

  // Notification balloon.
  DesktopNotificationBalloon notification_;

  // Status menu shown when right-clicking the system icon, if it has been
  // created by |UpdatePlatformContextMenu|.
  MenuControllerCocoa* __strong menu_;

  // Boolean which determines whether the menu should be opened with secondary
  // clicks. When true, left click will dispatch a click event even if a menu
  // exists, and right click/control-click will open the menu. Additionally, the
  // tooltip will be shown when hovering the status icon and not as an entry in
  // the menu.
  bool open_menu_with_secondary_click_ = false;

  // Stores the menu model so that it can be created later in
  // CreateAndOpenMenu() or so its observers can be cleaned up.
  raw_ptr<StatusIconMenuModel> menu_model_ = nullptr;

  base::ScopedObservation<StatusIconMenuModel, StatusIconMenuModel::Observer>
      observation_{this};
};

#endif  // CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
