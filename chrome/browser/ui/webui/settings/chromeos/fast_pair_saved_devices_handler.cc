// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/fast_pair_saved_devices_handler.h"

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder_impl.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

const char kLoadSavedDevicePage[] = "loadSavedDevicePage";
const char kRemoveSavedDevice[] = "removeSavedDevice";
const char kOptInStatusMessage[] = "fast-pair-saved-devices-opt-in-status";
const char kSavedDevicesListMessage[] = "fast-pair-saved-devices-list";

std::string DecodeKey(const std::string& encoded_key) {
  std::string key;
  base::Base64Decode(encoded_key, &key);
  QP_LOG(ERROR) << __func__ << ": " << encoded_key << " " << key;
  return key;
}

std::string EncodeKey(const std::string& decoded_key) {
  return base::HexEncode(
      std::vector<uint8_t>(decoded_key.begin(), decoded_key.end()));
}

// Keys in the JSON representation of a SavedDevice
const char kSavedDeviceNameKey[] = "name";
const char kSavedDeviceImageUrlKey[] = "imageUrl";
const char kSavedDeviceAccountKeyKey[] = "accountKey";

// Converts |device| to a raw dictionary value used as a JSON
// argument to JavaScript functions.
base::Value SavedDeviceToDictionary(const std::string& device_name,
                                    const std::string& image_url,
                                    const std::string account_key) {
  base::Value::Dict dictionary;
  dictionary.Set(kSavedDeviceNameKey, device_name);
  dictionary.Set(kSavedDeviceImageUrlKey, image_url);
  dictionary.Set(kSavedDeviceAccountKeyKey, EncodeKey(account_key));
  return base::Value(std::move(dictionary));
}

}  // namespace

namespace chromeos::settings {

FastPairSavedDevicesHandler::FastPairSavedDevicesHandler()
    : image_decoder_(
          std::make_unique<ash::quick_pair::FastPairImageDecoderImpl>()) {}

FastPairSavedDevicesHandler::FastPairSavedDevicesHandler(
    std::unique_ptr<ash::quick_pair::FastPairImageDecoder> image_decoder)
    : image_decoder_(std::move(image_decoder)) {}

FastPairSavedDevicesHandler::~FastPairSavedDevicesHandler() = default;

void FastPairSavedDevicesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kLoadSavedDevicePage,
      base::BindRepeating(
          &FastPairSavedDevicesHandler::HandleLoadSavedDevicePage,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kRemoveSavedDevice,
      base::BindRepeating(&FastPairSavedDevicesHandler::HandleRemoveSavedDevice,
                          base::Unretained(this)));
}

void FastPairSavedDevicesHandler::HandleRemoveSavedDevice(
    const base::Value::List& args) {
  std::vector<uint8_t> account_key;
  base::HexStringToBytes(args[0].GetString(), &account_key);
  ash::quick_pair::FastPairRepository::Get()
      ->DeleteAssociatedDeviceByAccountKey(
          account_key,
          base::BindOnce(&FastPairSavedDevicesHandler::OnSavedDeviceDeleted,
                         weak_ptr_factory_.GetWeakPtr()));
}

void FastPairSavedDevicesHandler::OnSavedDeviceDeleted(bool success) {
  QP_LOG(ERROR) << __func__ << ": " << (success ? "success" : "failed");
}

void FastPairSavedDevicesHandler::HandleLoadSavedDevicePage(
    const base::Value::List& args) {
  AllowJavascript();

  // If the page is already loading, we ignore any new requests to load the
  // page.
  if (loading_saved_device_page_)
    return;

  loading_saved_device_page_ = true;
  ash::quick_pair::FastPairRepository::Get()->GetSavedDevices(
      base::BindOnce(&FastPairSavedDevicesHandler::OnGetSavedDevices,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairSavedDevicesHandler::OnGetSavedDevices(
    nearby::fastpair::OptInStatus status,
    std::vector<nearby::fastpair::FastPairDevice> devices) {
  // The JavaScript WebUI layer needs an enum to stay in sync with the
  // nearby::fastpair::OptInStatus enum from ash/quick_pair/proto/enums.proto
  // to properly handle this message.
  FireWebUIListener(kOptInStatusMessage, base::Value(static_cast<int>(status)));

  // If the device list is empty, we communicate it to the settings page to
  // stop the loading UX screen.
  if (devices.empty()) {
    base::Value::List saved_devices_list;
    FireWebUIListener(kSavedDevicesListMessage,
                      base::Value(std::move(saved_devices_list)));
    loading_saved_device_page_ = false;
    return;
  }

  // The difference between a nearby::fastpair::FastPairDevice and a
  // nearby::fastpair::StoredDiscoveryItem is that although they represent the
  // same devices saved to a user's account, the FastPairDevice contains the
  // raw byte array with the device name and the image url, which we will
  // parse into a StoredDiscoveryItem to access the image url as strings. We
  // create a flat set of these strings in the case that the
  // user has the same device multiple times, we only decode the image url
  // once.
  devices_ = devices;
  base::flat_set<std::string> image_urls;
  for (const auto& device : devices) {
    nearby::fastpair::StoredDiscoveryItem item;
    if (item.ParseFromString(device.discovery_item_bytes()) &&
        item.has_display_url()) {
      image_urls.insert(item.display_url());
    }
  }

  // Image decoding occurs asynchronously in a separate process, so we use
  // a AtomicRefCounter to keep track of the pending tasks remaining, and once
  // they complete, we can continue parsing the saved device data to communicate
  // with the settings page.
  pending_decoding_tasks_count_ =
      std::make_unique<base::AtomicRefCount>(image_urls.size());
  for (std::string image_url : image_urls) {
    image_decoder_->DecodeImageFromUrl(
        GURL(image_url),
        /*resize_to_notification_size=*/false,
        base::BindOnce(&FastPairSavedDevicesHandler::SaveImageAsBase64,
                       weak_ptr_factory_.GetWeakPtr(), image_url));
  }
}

void FastPairSavedDevicesHandler::SaveImageAsBase64(
    const std::string& image_url,
    gfx::Image image) {
  if (!image.IsEmpty()) {
    std::string encoded_image = webui::GetBitmapDataUrl(image.AsBitmap());
    image_url_to_encoded_url_map_[image_url] = encoded_image;
  }

  // Even if the image is empty, we want to decrement our task counter to
  // handle when the tasks are completed, and ultimately communicate the
  // list to the settings page. If we don't have an image, we will
  // send the settings page a null url, and it is up to the settings page
  // to handle this case as needed.
  if (!pending_decoding_tasks_count_->Decrement())
    DecodingUrlsFinished();
}

void FastPairSavedDevicesHandler::DecodingUrlsFinished() {
  // We initialize a list of the saved devices that we will parse with the
  // decoded urls we have.
  base::Value::List saved_devices_list;
  saved_devices_list.reserve(devices_.size());

  // |nearby::fastpair::StoredDiscoveryItem| contains information about
  // the device name, |image_url_to_encoded_url_map_| contains the base64
  // encoded urls for the images, and |nearby::fastpair::FastPairDevice|
  // contains the account key. Here, we reconcile this data for each device
  // and convert it to dictionary, to be communicated to the settings page.
  for (const auto& device : devices_) {
    // If the device does not have an account key, then it was not properly
    // saved to the user's account, and we ignore these devices.
    if (!device.has_account_key())
      continue;

    std::string account_key = device.account_key();
    std::string image_url = "";
    nearby::fastpair::StoredDiscoveryItem item;
    if (item.ParseFromString(device.discovery_item_bytes()) &&
        item.has_display_url() &&
        base::Contains(image_url_to_encoded_url_map_, item.display_url())) {
      image_url = image_url_to_encoded_url_map_[item.display_url()];
    }

    saved_devices_list.Append(SavedDeviceToDictionary(
        /*device_name=*/item.has_device_name() ? item.device_name() : "",
        /*image_url=*/image_url,
        /*account_key=*/account_key));
  }

  FireWebUIListener(kSavedDevicesListMessage,
                    base::Value(std::move(saved_devices_list)));

  // We reset the state here for another page load that may happened while
  // chrome://os-settings is open, since our decoding tasks are completed.
  devices_.clear();
  image_url_to_encoded_url_map_.clear();
  pending_decoding_tasks_count_.reset();
  loading_saved_device_page_ = false;
}

void FastPairSavedDevicesHandler::OnJavascriptAllowed() {}

void FastPairSavedDevicesHandler::OnJavascriptDisallowed() {}

}  // namespace chromeos::settings
