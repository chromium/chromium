// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/bluetooth/fast_pair_saved_devices_handler.h"

#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"
#include "ash/quick_pair/repository/fast_pair/fast_pair_image_decoder_impl.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/cross_device/logging/logging.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

const char kLoadSavedDevicePage[] = "loadSavedDevicePage";
const char kRemoveSavedDevice[] = "removeSavedDevice";
const char kOptInStatusMessage[] = "fast-pair-saved-devices-opt-in-status";
const char kSavedDevicesListMessage[] = "fast-pair-saved-devices-list";

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
base::Value::Dict SavedDeviceToDictionary(const std::string& device_name,
                                          const std::string& image_url,
                                          const std::string account_key) {
  return base::Value::Dict()
      .Set(kSavedDeviceNameKey, device_name)
      .Set(kSavedDeviceImageUrlKey, image_url)
      .Set(kSavedDeviceAccountKeyKey, EncodeKey(account_key));
}

}  // namespace

namespace ash::settings {

FastPairSavedDevicesHandler::FastPairSavedDevicesHandler()
    : image_decoder_(std::make_unique<quick_pair::FastPairImageDecoderImpl>()) {
}

FastPairSavedDevicesHandler::FastPairSavedDevicesHandler(
    std::unique_ptr<quick_pair::FastPairImageDecoder> image_decoder)
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
  CD_LOG(VERBOSE, Feature::FP) << __func__;
  std::vector<uint8_t> account_key;
  base::HexStringToBytes(args[0].GetString(), &account_key);
  quick_pair::FastPairRepository::Get()->DeleteAssociatedDeviceByAccountKey(
      account_key,
      base::BindOnce(&FastPairSavedDevicesHandler::OnSavedDeviceDeleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairSavedDevicesHandler::OnSavedDeviceDeleted(bool success) {
  CD_LOG(INFO, Feature::FP)
      << __func__ << ": " << (success ? "success" : "failed");
  quick_pair::RecordSavedDevicesRemoveResult(/*success=*/success);
}

void FastPairSavedDevicesHandler::HandleLoadSavedDevicePage(
    const base::Value::List& args) {
  CD_LOG(VERBOSE, Feature::FP) << __func__;
  AllowJavascript();

  // If the page is already loading, we ignore any new requests to load the
  // page.
  if (loading_saved_device_page_) {
    return;
  }

  loading_saved_device_page_ = true;
  loading_start_time_ = base::TimeTicks::Now();
  quick_pair::FastPairRepository::Get()->GetSavedDevices(
      base::BindOnce(&FastPairSavedDevicesHandler::OnGetSavedDevices,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastPairSavedDevicesHandler::OnGetSavedDevices(
    nearby::fastpair::OptInStatus status,
    std::vector<nearby::fastpair::FastPairDevice> devices) {
  CD_LOG(VERBOSE, Feature::FP) << __func__;

  // The JavaScript WebUI layer needs an enum to stay in sync with the
  // nearby::fastpair::OptInStatus enum from ash/quick_pair/proto/enums.proto
  // to properly handle this message.
  FireWebUIListener(kOptInStatusMessage, base::Value(static_cast<int>(status)));
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": Sending opt-in status of " << status;

  // If the device list is empty, we communicate it to the settings page to
  // stop the loading UX screen.
  if (devices.empty()) {
    CD_LOG(VERBOSE, Feature::FP)
        << __func__ << ": No devices saved to the user's account";
    base::Value::List saved_devices_list;
    quick_pair::RecordSavedDevicesCount(
        /*num_devices=*/saved_devices_list.size());
    FireWebUIListener(kSavedDevicesListMessage, saved_devices_list);
    loading_saved_device_page_ = false;
    base::TimeDelta total_load_time =
        base::TimeTicks::Now() - loading_start_time_;
    quick_pair::RecordSavedDevicesTotalUxLoadTime(total_load_time);
    return;
  }

  // The difference between a nearby::fastpair::FastPairDevice and a
  // nearby::fastpair::StoredDiscoveryItem is that although they represent the
  // same devices saved to a user's account, the FastPairDevice contains the
  // raw byte array with the device name and the image bytes, which we will
  // parse into a StoredDiscoveryItem to access the image bytes as strings. We
  // create a flat set of these strings in the case that the
  // user has the same device multiple times, we only decode the image bytes
  // once.
  devices_ = devices;
  base::flat_set<std::string> image_byte_strings;
  for (const auto& device : devices) {
    nearby::fastpair::StoredDiscoveryItem item;
    if (item.ParseFromString(device.discovery_item_bytes()) &&
        item.has_icon_png()) {
      image_byte_strings.insert(item.icon_png());
    }
  }

  if (image_byte_strings.empty()) {
    CD_LOG(VERBOSE, Feature::FP) << __func__ << ": no device images";
    DecodingUrlsFinished();
    return;
  }

  // Image decoding occurs asynchronously in a separate process, so we use
  // a AtomicRefCounter to keep track of the pending tasks remaining, and once
  // they complete, we can continue parsing the saved device data to communicate
  // with the settings page.
  pending_decoding_tasks_count_ =
      std::make_unique<base::AtomicRefCount>(image_byte_strings.size());
  for (std::string image_byte_string : image_byte_strings) {
    image_decoder_->DecodeImage(
        std::vector<uint8_t>(image_byte_string.begin(),
                             image_byte_string.end()),
        /*resize_to_notification_size=*/false,
        base::BindOnce(&FastPairSavedDevicesHandler::SaveImageAsBase64,
                       weak_ptr_factory_.GetWeakPtr(), image_byte_string));
  }
}

void FastPairSavedDevicesHandler::SaveImageAsBase64(
    const std::string& image_byte_string,
    gfx::Image image) {
  if (!image.IsEmpty()) {
    std::string encoded_image = webui::GetBitmapDataUrl(image.AsBitmap());
    image_byte_string_to_encoded_url_map_[image_byte_string] = encoded_image;
  }

  // Even if the image is empty, we want to decrement our task counter to
  // handle when the tasks are completed, and ultimately communicate the
  // list to the settings page. If we don't have an image, we will
  // send the settings page a null url, and it is up to the settings page
  // to handle this case as needed.
  if (!pending_decoding_tasks_count_->Decrement()) {
    DecodingUrlsFinished();
  }
}

void FastPairSavedDevicesHandler::DecodingUrlsFinished() {
  CD_LOG(VERBOSE, Feature::FP) << __func__;

  // We initialize a list of the saved devices that we will parse with the
  // decoded urls we have.
  base::Value::List saved_devices_list;
  saved_devices_list.reserve(devices_.size());

  // |nearby::fastpair::StoredDiscoveryItem| contains information about
  // the device name, |image_byte_string_to_encoded_url_map_| contains the
  // base64 encoded urls for the images, and |nearby::fastpair::FastPairDevice|
  // contains the account key. Here, we reconcile this data for each device
  // and convert it to dictionary, to be communicated to the settings page.
  for (const auto& device : devices_) {
    // If the device does not have an account key, then it was not properly
    // saved to the user's account, and we ignore these devices. Android
    // has devices still in Footprints but marked as deleted by removing the
    // account key, so it is not expected for all of these devices to have
    // account keys.
    if (!device.has_account_key()) {
      continue;
    }

    std::string account_key = device.account_key();
    std::string image_url = "";
    nearby::fastpair::StoredDiscoveryItem item;
    if (item.ParseFromString(device.discovery_item_bytes()) &&
        item.has_icon_png() &&
        base::Contains(image_byte_string_to_encoded_url_map_,
                       item.icon_png())) {
      image_url = image_byte_string_to_encoded_url_map_[item.icon_png()];
    }

    saved_devices_list.Append(SavedDeviceToDictionary(
        /*device_name=*/item.has_title() ? item.title() : "",
        /*image_url=*/image_url,
        /*account_key=*/account_key));
  }

  quick_pair::RecordSavedDevicesCount(
      /*num_devices=*/saved_devices_list.size());
  FireWebUIListener(kSavedDevicesListMessage, saved_devices_list);
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": Sending device list";
  base::TimeDelta total_load_time =
      base::TimeTicks::Now() - loading_start_time_;
  quick_pair::RecordSavedDevicesTotalUxLoadTime(total_load_time);

  // We reset the state here for another page load that may happened while
  // chrome://os-settings is open, since our decoding tasks are completed.
  devices_.clear();
  image_byte_string_to_encoded_url_map_.clear();
  pending_decoding_tasks_count_.reset();
  loading_saved_device_page_ = false;
}

void FastPairSavedDevicesHandler::OnJavascriptAllowed() {}

void FastPairSavedDevicesHandler::OnJavascriptDisallowed() {}

}  // namespace ash::settings
