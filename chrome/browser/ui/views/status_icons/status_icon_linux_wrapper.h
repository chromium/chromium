// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_WRAPPER_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_WRAPPER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/status_icons/desktop_notification_balloon.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "ui/views/linux_ui/status_icon_linux.h"

class StatusIconLinuxDbus;

// Wrapper class for StatusIconLinux that implements the standard StatusIcon
// interface. Also handles callbacks from StatusIconLinux.
class StatusIconLinuxWrapper : public StatusIcon,
                               public views::StatusIconLinux::Delegate,
                               public StatusIconMenuModel::Observer {
 public:
  ~StatusIconLinuxWrapper() override;

  // StatusIcon overrides:
  void SetImage(const gfx::ImageSkia& image) override;
  void SetToolTip(const base::string16& tool_tip) override;
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const base::string16& title,
                      const base::string16& contents,
                      const message_center::NotifierId& notifier_id) override;

  // StatusIconLinux::Delegate overrides:
  void OnClick() override;
  bool HasClickAction() override;
  const gfx::ImageSkia& GetImage() const override;
  const base::string16& GetToolTip() const override;
  ui::MenuModel* GetMenuModel() const override;
  void OnImplInitializationFailed() override;

  // StatusIconMenuModel::Observer overrides:
  void OnMenuStateChanged() override;

  static std::unique_ptr<StatusIconLinuxWrapper> CreateWrappedStatusIcon(
      const gfx::ImageSkia& image,
      const base::string16& tool_tip);

 protected:
  // StatusIcon overrides:
  // Invoked after a call to SetContextMenu() to let the platform-specific
  // subclass update the native context menu based on the new model. If NULL is
  // passed, subclass should destroy the native context menu.
  void UpdatePlatformContextMenu(StatusIconMenuModel* model) override;

 private:
  enum StatusIconType {
    kTypeDbus,
    kTypeX11,
    kTypeNone,
  };

  // A status icon wrapper should only be created by calling
  // CreateWrappedStatusIcon().
  StatusIconLinuxWrapper(views::StatusIconLinux* status_icon,
                         StatusIconType status_icon_type,
                         const gfx::ImageSkia& image,
                         const base::string16& tool_tip);
#if defined(USE_DBUS)
  StatusIconLinuxWrapper(scoped_refptr<StatusIconLinuxDbus> status_icon,
                         const gfx::ImageSkia& image,
                         const base::string16& tool_tip);
#endif
  StatusIconLinuxWrapper(std::unique_ptr<views::StatusIconLinux> status_icon,
                         StatusIconType status_icon_type,
                         const gfx::ImageSkia& image,
                         const base::string16& tool_tip);

  // Notification balloon.
  DesktopNotificationBalloon notification_;

  // The status icon may be ref-counted (via |status_icon_dbus_|) or owned by
  // |this| (via |status_icon_linux_|).  Either way, |status_icon_| points to
  // the underlying object.
#if defined(USE_DBUS)
  scoped_refptr<StatusIconLinuxDbus> status_icon_dbus_;
#endif
  std::unique_ptr<views::StatusIconLinux> status_icon_linux_;
  views::StatusIconLinux* status_icon_;
  StatusIconType status_icon_type_;

  gfx::ImageSkia image_;
  base::string16 tool_tip_;
  StatusIconMenuModel* menu_model_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(StatusIconLinuxWrapper);
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_WRAPPER_H_
