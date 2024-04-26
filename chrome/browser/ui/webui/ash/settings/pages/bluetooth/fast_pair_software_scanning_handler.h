// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_BLUETOOTH_FAST_PAIR_SOFTWARE_SCANNING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_BLUETOOTH_FAST_PAIR_SOFTWARE_SCANNING_HANDLER_H_

#include "ash/quick_pair/feature_status_tracker/battery_saver_active_provider.h"
#include "ash/quick_pair/feature_status_tracker/hardware_offloading_supported_provider.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace ash::settings {

class FastPairSoftwareScanningHandler
    : public ::settings::SettingsPageUIHandler {
 public:
  FastPairSoftwareScanningHandler(
      std::unique_ptr<ash::quick_pair::BatterySaverActiveProvider>
          battery_saver_active_provider,
      std::unique_ptr<ash::quick_pair::HardwareOffloadingSupportedProvider>
          hardware_offloading_supported_provider);
  FastPairSoftwareScanningHandler(const FastPairSoftwareScanningHandler&) =
      delete;
  FastPairSoftwareScanningHandler& operator=(
      const FastPairSoftwareScanningHandler&) = delete;
  ~FastPairSoftwareScanningHandler() override;

  // WebUIMessageHandler
  void RegisterMessages() override;

  // SettingsPageUIHandler
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleBatterySaverActiveStatusRequest(const base::Value::List& args);
  void HandleHardwareOffloadingSupportStatusRequest(
      const base::Value::List& args);
  void OnBatterySaverActiveStatusChange(bool is_enabled);
  void OnHardwareOffloadingSupportedStatusChange(bool is_enabled);

  std::unique_ptr<ash::quick_pair::BatterySaverActiveProvider>
      battery_saver_active_provider_;
  std::unique_ptr<ash::quick_pair::HardwareOffloadingSupportedProvider>
      hardware_offloading_supported_provider_;
  base::WeakPtrFactory<FastPairSoftwareScanningHandler> weak_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_BLUETOOTH_FAST_PAIR_SOFTWARE_SCANNING_HANDLER_H_
