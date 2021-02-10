// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_POINTER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_POINTER_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/system/pointer_device_observer.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace base {
class ListValue;
}

namespace chromeos {
namespace settings {

// Chrome OS "Mouse and touchpad" settings page UI handler.
class PointerHandler
    : public ::settings::SettingsPageUIHandler,
      public chromeos::system::PointerDeviceObserver::Observer {
 public:
  PointerHandler();
  ~PointerHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // PointerDeviceObserver implementation.
  void TouchpadExists(bool exists) override;
  void MouseExists(bool exists) override;
  void PointingStickExists(bool exists) override;

  // Initializes the page with the current pointer information.
  void HandleInitialize(const base::ListValue* args);

  std::unique_ptr<chromeos::system::PointerDeviceObserver>
      pointer_device_observer_;

  DISALLOW_COPY_AND_ASSIGN(PointerHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_POINTER_HANDLER_H_
