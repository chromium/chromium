// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
#define CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
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
  void SetVisible(bool visible);

  // StatusIconMenuModel::Observer overrides:
  void OnMenuStateChanged() override;

  bool HasStatusIconMenu();

  bool open_menu_with_secondary_click() {
    return open_menu_with_secondary_click_;
  }

  // Posts a deferred `menu_` rebuild if a model state change arrived while
  // `menu_` was open.
  void OnMenuDidEndTracking();

  // Runs `open_menu_for_testing_` and returns true if a test intercepted
  // opening `item_.menu`.
  bool MaybeRunOpenMenuForTesting();

  NSStatusItem* GetItemForTesting() { return item(); }
  void SetOpenMenuForTesting(base::RepeatingClosure callback);

 protected:
  // Overridden from StatusIcon.
  void UpdatePlatformContextMenu(StatusIconMenuModel* model) override;

 private:
  void CreateMenu(ui::MenuModel* model);
  void FlushDeferredMenuRebuild();

  // Getter for item_ that allows lazy initialization.
  NSStatusItem* item();
  NSStatusItem* __strong item_;

  // Notification balloon.
  DesktopNotificationBalloon notification_;

  // Status menu attached to `item_` when created by
  // `UpdatePlatformContextMenu`.
  MenuControllerCocoa* __strong menu_;

  // Boolean which determines whether the menu should be opened with secondary
  // clicks. When true, plain left clicks dispatch a click event even if a menu
  // exists, while right clicks, control-clicks, VoiceOver (AXPress), and
  // Ctrl+F8 status menu navigation open `item_.menu`.
  bool open_menu_with_secondary_click_ = false;

  // Set when `OnMenuStateChanged` fires while `menu_` is actively tracking, so
  // `menu_` is rebuilt once tracking finishes instead of cancelling the open
  // menu.
  bool menu_needs_rebuild_ = false;

  base::RepeatingClosure open_menu_for_testing_;

  base::ScopedObservation<StatusIconMenuModel, StatusIconMenuModel::Observer>
      observation_{this};

  base::WeakPtrFactory<StatusIconMac> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
