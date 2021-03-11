// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/bluetooth_chooser_controller.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/net/referrer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/bluetooth/bluetooth_chooser_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/webui_url_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_MAC)
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#endif

namespace {

#if defined(OS_MAC)
static constexpr char kBluetoothSettingsUri[] =
    "x-apple.systempreferences:com.apple.preference.security?Privacy_"
    "Bluetooth";
#endif

Browser* GetBrowser() {
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      ProfileManager::GetLastUsedProfileAllowedByPolicy());
  DCHECK(browser_displayer.browser());
  return browser_displayer.browser();
}

void RecordInteractionWithChooser(bool has_null_handler) {
  UMA_HISTOGRAM_BOOLEAN("Bluetooth.Web.ChooserInteraction", has_null_handler);
}

}  // namespace

BluetoothChooserController::BluetoothChooserController(
    content::RenderFrameHost* owner,
    const content::BluetoothChooser::EventHandler& event_handler)
    : ChooserController(owner,
                        IDS_BLUETOOTH_DEVICE_CHOOSER_PROMPT_ORIGIN,
                        IDS_BLUETOOTH_DEVICE_CHOOSER_PROMPT_EXTENSION_NAME),
      event_handler_(event_handler) {
  if (owner) {
    frame_tree_node_id_ = owner->GetFrameTreeNodeId();
  }
}

BluetoothChooserController::~BluetoothChooserController() {}

bool BluetoothChooserController::ShouldShowIconBeforeText() const {
  return true;
}

bool BluetoothChooserController::ShouldShowReScanButton() const {
  return true;
}

std::u16string BluetoothChooserController::GetNoOptionsText() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_NO_DEVICES_FOUND_PROMPT);
}

std::u16string BluetoothChooserController::GetOkButtonLabel() const {
  return l10n_util::GetStringUTF16(
      IDS_BLUETOOTH_DEVICE_CHOOSER_PAIR_BUTTON_TEXT);
}

std::pair<std::u16string, std::u16string>
BluetoothChooserController::GetThrobberLabelAndTooltip() const {
  return {
      l10n_util::GetStringUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING_LABEL),
      l10n_util::GetStringUTF16(
          IDS_BLUETOOTH_DEVICE_CHOOSER_SCANNING_LABEL_TOOLTIP)};
}

size_t BluetoothChooserController::NumOptions() const {
  return devices_.size();
}

int BluetoothChooserController::GetSignalStrengthLevel(size_t index) const {
  return devices_[index].signal_strength_level;
}

bool BluetoothChooserController::IsConnected(size_t index) const {
  return devices_[index].is_connected;
}

bool BluetoothChooserController::IsPaired(size_t index) const {
  return devices_[index].is_paired;
}

std::u16string BluetoothChooserController::GetOption(size_t index) const {
  DCHECK_LT(index, devices_.size());
  const std::string& device_id = devices_[index].id;
  const auto& device_name_it = device_id_to_name_map_.find(device_id);
  DCHECK(device_name_it != device_id_to_name_map_.end());
  const auto& it = device_name_counts_.find(device_name_it->second);
  DCHECK(it != device_name_counts_.end());
  return it->second == 1
             ? device_name_it->second
             : l10n_util::GetStringFUTF16(
                   IDS_DEVICE_CHOOSER_DEVICE_NAME_WITH_ID,
                   device_name_it->second, base::UTF8ToUTF16(device_id));
}

void BluetoothChooserController::RefreshOptions() {
  RecordInteractionWithChooser(event_handler_.is_null());
  if (event_handler_.is_null())
    return;
  ClearAllDevices();
  event_handler_.Run(content::BluetoothChooserEvent::RESCAN, std::string());
}

void BluetoothChooserController::OpenAdapterOffHelpUrl() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Chrome OS can directly link to the OS setting to turn on the adapter.
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
      GetBrowser()->profile(),
      chromeos::settings::mojom::kBluetoothDevicesSubpagePath);
#else
  // For other operating systems, show a help center page in a tab.
  GetBrowser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kBluetoothAdapterOffHelpURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false /* is_renderer_initialized */));
#endif
}

void BluetoothChooserController::OpenPermissionPreferences() const {
#if defined(OS_MAC)
  content::WebContents* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  if (web_contents) {
    ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(
        GURL(kBluetoothSettingsUri), web_contents);
  }
#else
  NOTREACHED();
#endif
}

void BluetoothChooserController::Select(const std::vector<size_t>& indices) {
  DCHECK_EQ(1u, indices.size());
  size_t index = indices[0];
  RecordInteractionWithChooser(event_handler_.is_null());
  if (event_handler_.is_null()) {
    return;
  }
  DCHECK_LT(index, devices_.size());
  event_handler_.Run(content::BluetoothChooserEvent::SELECTED,
                     devices_[index].id);
}

void BluetoothChooserController::Cancel() {
  RecordInteractionWithChooser(event_handler_.is_null());
  if (event_handler_.is_null())
    return;
  event_handler_.Run(content::BluetoothChooserEvent::CANCELLED, std::string());
}

void BluetoothChooserController::Close() {
  RecordInteractionWithChooser(event_handler_.is_null());
  if (event_handler_.is_null())
    return;
  event_handler_.Run(content::BluetoothChooserEvent::CANCELLED, std::string());
}

void BluetoothChooserController::OpenHelpCenterUrl() const {
  GetBrowser()->OpenURL(content::OpenURLParams(
      GURL(chrome::kChooserBluetoothOverviewURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false /* is_renderer_initialized */));
}

void BluetoothChooserController::OnAdapterPresenceChanged(
    content::BluetoothChooser::AdapterPresence presence) {
  ClearAllDevices();
  switch (presence) {
    case content::BluetoothChooser::AdapterPresence::ABSENT:
      NOTREACHED();
      break;
    case content::BluetoothChooser::AdapterPresence::POWERED_OFF:
      if (view()) {
        view()->OnAdapterEnabledChanged(
            false /* Bluetooth adapter is turned off */);
      }
      break;
    case content::BluetoothChooser::AdapterPresence::POWERED_ON:
      if (view()) {
        view()->OnAdapterEnabledChanged(
            true /* Bluetooth adapter is turned on */);
      }
      break;
    case content::BluetoothChooser::AdapterPresence::UNAUTHORIZED:
      if (view()) {
        view()->OnAdapterAuthorizationChanged(/*authorized=*/false);
      }
      break;
  }
}

void BluetoothChooserController::OnDiscoveryStateChanged(
    content::BluetoothChooser::DiscoveryState state) {
  switch (state) {
    case content::BluetoothChooser::DiscoveryState::DISCOVERING:
      if (view()) {
        view()->OnRefreshStateChanged(
            true /* Refreshing options is in progress */);
      }
      break;
    case content::BluetoothChooser::DiscoveryState::IDLE:
    case content::BluetoothChooser::DiscoveryState::FAILED_TO_START:
      if (view()) {
        view()->OnRefreshStateChanged(
            false /* Refreshing options is complete */);
      }
      break;
  }
}

void BluetoothChooserController::AddOrUpdateDevice(
    const std::string& device_id,
    bool should_update_name,
    const std::u16string& device_name,
    bool is_gatt_connected,
    bool is_paired,
    int signal_strength_level) {
  auto name_it = device_id_to_name_map_.find(device_id);
  if (name_it != device_id_to_name_map_.end()) {
    if (should_update_name) {
      std::u16string previous_device_name = name_it->second;
      name_it->second = device_name;

      const auto& it = device_name_counts_.find(previous_device_name);
      DCHECK(it != device_name_counts_.end());
      DCHECK_GT(it->second, 0);

      if (--(it->second) == 0)
        device_name_counts_.erase(it);

      ++device_name_counts_[device_name];
    }

    auto device_it =
        std::find_if(devices_.begin(), devices_.end(),
                     [&device_id](const BluetoothDeviceInfo& device) {
                       return device.id == device_id;
                     });

    DCHECK(device_it != devices_.end());
    // When Bluetooth device scanning stops, the |signal_strength_level|
    // is -1, and in this case, should still use the previously stored
    // signal strength level value.
    if (signal_strength_level != -1)
      device_it->signal_strength_level = signal_strength_level;
    device_it->is_connected = is_gatt_connected;
    device_it->is_paired = is_paired;
    if (view())
      view()->OnOptionUpdated(device_it - devices_.begin());
    return;
  }

  devices_.push_back(
      {device_id, signal_strength_level, is_gatt_connected, is_paired});
  device_id_to_name_map_.insert({device_id, device_name});
  ++device_name_counts_[device_name];
  if (view())
    view()->OnOptionAdded(devices_.size() - 1);
}

void BluetoothChooserController::RemoveDevice(const std::string& device_id) {
  const auto& name_it = device_id_to_name_map_.find(device_id);
  if (name_it == device_id_to_name_map_.end())
    return;

  auto device_it =
      std::find_if(devices_.begin(), devices_.end(),
                   [&device_id](const BluetoothDeviceInfo& device) {
                     return device.id == device_id;
                   });

  if (device_it != devices_.end()) {
    size_t index = device_it - devices_.begin();
    devices_.erase(device_it);

    const auto& it = device_name_counts_.find(name_it->second);
    DCHECK(it != device_name_counts_.end());
    DCHECK_GT(it->second, 0);

    if (--(it->second) == 0)
      device_name_counts_.erase(it);

    device_id_to_name_map_.erase(name_it);

    if (view())
      view()->OnOptionRemoved(index);
  }
}

void BluetoothChooserController::ResetEventHandler() {
  event_handler_.Reset();
}

base::WeakPtr<BluetoothChooserController>
BluetoothChooserController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void BluetoothChooserController::ClearAllDevices() {
  devices_.clear();
  device_id_to_name_map_.clear();
  device_name_counts_.clear();
}
