// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_BLUETOOTH_FAST_PAIR_SAVED_DEVICES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_BLUETOOTH_FAST_PAIR_SAVED_DEVICES_HANDLER_H_

#include "ash/quick_pair/proto/fastpair.pb.h"
#include "base/atomic_ref_count.h"
#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "ui/gfx/image/image.h"

namespace ash {

namespace quick_pair {
class FastPairImageDecoder;
}

namespace settings {

// Chrome OS Fast Pair Saved Devices subpage UI handler.
class FastPairSavedDevicesHandler : public ::settings::SettingsPageUIHandler {
 public:
  FastPairSavedDevicesHandler();
  explicit FastPairSavedDevicesHandler(
      std::unique_ptr<quick_pair::FastPairImageDecoder> image_decoder);
  FastPairSavedDevicesHandler(const FastPairSavedDevicesHandler&) = delete;
  FastPairSavedDevicesHandler& operator=(const FastPairSavedDevicesHandler&) =
      delete;
  ~FastPairSavedDevicesHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleLoadSavedDevicePage(const base::Value::List& args);
  void OnGetSavedDevices(nearby::fastpair::OptInStatus status,
                         std::vector<nearby::fastpair::FastPairDevice> devices);
  void SaveImageAsBase64(const std::string& image_byte_string,
                         gfx::Image image);
  void DecodingUrlsFinished();

  void HandleRemoveSavedDevice(const base::Value::List& args);
  void OnSavedDeviceDeleted(bool success);

  bool loading_saved_device_page_ = false;
  base::TimeTicks loading_start_time_;

  std::unique_ptr<base::AtomicRefCount> pending_decoding_tasks_count_;
  std::vector<nearby::fastpair::FastPairDevice> devices_;
  std::unique_ptr<quick_pair::FastPairImageDecoder> image_decoder_;

  // For each device image, we need to convert the device image from the proto
  // into a base64 encoded data URL to be displayed in the settings UX because
  // external image URLs cannot be directly downloaded into chrome://os-settings
  // for security reasons. This map is used to store the encoded urls created
  // in async calls to be used when we create our dictionary to give to
  // chrome://os-settings for display.
  base::flat_map<std::string, std::string>
      image_byte_string_to_encoded_url_map_;

  base::WeakPtrFactory<FastPairSavedDevicesHandler> weak_ptr_factory_{this};
};

}  // namespace settings
}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_BLUETOOTH_FAST_PAIR_SAVED_DEVICES_HANDLER_H_
