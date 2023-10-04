// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DEVICE_POINTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DEVICE_POINTER_HANDLER_H_

#include <memory>

#include "chrome/browser/ash/system/pointer_device_observer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace ash::settings {

// Chrome OS "Mouse and touchpad" settings page UI handler.
class PointerHandler : public ::settings::SettingsPageUIHandler,
                       public system::PointerDeviceObserver::Observer {
 public:
  PointerHandler();

  PointerHandler(const PointerHandler&) = delete;
  PointerHandler& operator=(const PointerHandler&) = delete;

  ~PointerHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // PointerDeviceObserver implementation.
  void TouchpadExists(bool exists) override;
  void HapticTouchpadExists(bool exists) override;
  void MouseExists(bool exists) override;
  void PointingStickExists(bool exists) override;

  // Initializes the page with the current pointer information.
  void HandleInitialize(const base::Value::List& args);

  std::unique_ptr<system::PointerDeviceObserver> pointer_device_observer_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_DEVICE_POINTER_HANDLER_H_
