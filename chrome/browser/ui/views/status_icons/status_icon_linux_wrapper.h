// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_WRAPPER_H_
#define CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_WRAPPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/status_icons/desktop_notification_balloon.h"
#include "chrome/browser/status_icons/status_icon.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/linux/status_icon_linux.h"

class StatusIconLinuxDbus;

// Wrapper class for StatusIconLinux that implements the standard StatusIcon
// interface. Also handles callbacks from StatusIconLinux.
class StatusIconLinuxWrapper : public StatusIcon,
                               public ui::StatusIconLinux::Delegate,
                               public StatusIconMenuModel::Observer {
 public:
  StatusIconLinuxWrapper(const StatusIconLinuxWrapper&) = delete;
  StatusIconLinuxWrapper& operator=(const StatusIconLinuxWrapper&) = delete;

  ~StatusIconLinuxWrapper() override;

  // StatusIcon overrides:
  void SetImage(const gfx::ImageSkia& image) override;
  void SetToolTip(const std::u16string& tool_tip) override;
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const std::u16string& title,
                      const std::u16string& contents,
                      const message_center::NotifierId& notifier_id) override;

  // StatusIconLinux::Delegate overrides:
  void OnClick() override;
  bool HasClickAction() override;
  const gfx::ImageSkia& GetImage() const override;
  const std::u16string& GetToolTip() const override;
  ui::MenuModel* GetMenuModel() const override;
  void OnImplInitializationFailed() override;

  // StatusIconMenuModel::Observer overrides:
  void OnMenuStateChanged() override;

  static std::unique_ptr<StatusIconLinuxWrapper> CreateWrappedStatusIcon(
      const gfx::ImageSkia& image,
      const std::u16string& tool_tip);

 protected:
  // StatusIcon overrides:
  // Invoked after a call to SetContextMenu() to let the platform-specific
  // subclass update the native context menu based on the new model. If NULL is
  // passed, subclass should destroy the native context menu.
  void UpdatePlatformContextMenu(StatusIconMenuModel* model) override;

 private:
  enum StatusIconType {
#if defined(USE_DBUS)
    kTypeDbus,
#endif
    kTypeWindowed,
    kTypeNone,
  };

  // A status icon wrapper should only be created by calling
  // CreateWrappedStatusIcon().
  StatusIconLinuxWrapper(ui::StatusIconLinux* status_icon,
                         StatusIconType status_icon_type,
                         const gfx::ImageSkia& image,
                         const std::u16string& tool_tip);
#if defined(USE_DBUS)
  StatusIconLinuxWrapper(scoped_refptr<StatusIconLinuxDbus> status_icon,
                         const gfx::ImageSkia& image,
                         const std::u16string& tool_tip);
#endif
  StatusIconLinuxWrapper(std::unique_ptr<ui::StatusIconLinux> status_icon,
                         StatusIconType status_icon_type,
                         const gfx::ImageSkia& image,
                         const std::u16string& tool_tip);

  ui::StatusIconLinux* GetStatusIcon();

  // Notification balloon.
  DesktopNotificationBalloon notification_;

  // The status icon may be ref-counted (via |status_icon_dbus_|) or owned by
  // |this| (via |status_icon_linux_|).
#if defined(USE_DBUS)
  scoped_refptr<StatusIconLinuxDbus> status_icon_dbus_;
#endif
  std::unique_ptr<ui::StatusIconLinux> status_icon_linux_;
  StatusIconType status_icon_type_;

  gfx::ImageSkia image_;
  std::u16string tool_tip_;
  raw_ptr<StatusIconMenuModel> menu_model_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_STATUS_ICONS_STATUS_ICON_LINUX_WRAPPER_H_
