// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_policy_allowed_devices.h"

#include <optional>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "url/gurl.h"

namespace {

constexpr char kPrefDevicesKey[] = "devices";
constexpr char kPrefUrlsKey[] = "urls";
constexpr char kPrefVendorIdKey[] = "vendor_id";
constexpr char kPrefProductIdKey[] = "product_id";

}  // namespace

UsbPolicyAllowedDevices::UsbPolicyAllowedDevices(PrefService* pref_service) {
  pref_change_registrar_.Init(pref_service);
  // Add an observer for |kManagedWebUsbAllowDevicesForUrls| to call
  // CreateOrUpdateMap when the value is changed. The lifetime of
  // |pref_change_registrar_| is managed by this class, therefore it is safe to
  // use base::Unretained here.
  pref_change_registrar_.Add(
      prefs::kManagedWebUsbAllowDevicesForUrls,
      base::BindRepeating(&UsbPolicyAllowedDevices::CreateOrUpdateMap,
                          base::Unretained(this)));

  CreateOrUpdateMap();
}

UsbPolicyAllowedDevices::~UsbPolicyAllowedDevices() {}

bool UsbPolicyAllowedDevices::IsDeviceAllowed(
    const url::Origin& origin,
    const device::mojom::UsbDeviceInfo& device_info) const {
  return IsDeviceAllowed(
      origin, std::make_pair(device_info.vendor_id, device_info.product_id));
}

bool UsbPolicyAllowedDevices::IsDeviceAllowed(
    const url::Origin& origin,
    const std::pair<int, int>& device_ids) const {
  // Search through each set of URL pair that match the given device. The
  // keys correspond to the following URL pair sets:
  //  * (vendor_id, product_id): A set corresponding to the exact device.
  //  * (vendor_id, -1): A set corresponding to any device with |vendor_id|.
  //  * (-1, -1): A set corresponding to any device.
  const std::pair<int, int> set_keys[] = {
      std::make_pair(device_ids.first, device_ids.second),
      std::make_pair(device_ids.first, -1), std::make_pair(-1, -1)};

  for (const auto& key : set_keys) {
    const auto entry = usb_device_ids_to_urls_.find(key);
    if (entry == usb_device_ids_to_urls_.cend())
      continue;

    if (entry->second.find(origin) != entry->second.end())
      return true;
  }
  return false;
}

void UsbPolicyAllowedDevices::CreateOrUpdateMap() {
  const base::Value::List& pref_list = pref_change_registrar_.prefs()->GetList(
      prefs::kManagedWebUsbAllowDevicesForUrls);
  usb_device_ids_to_urls_.clear();

  // The pref value has already been validated by the policy handler, so it is
  // safe to assume that |pref_list| follows the policy template.
  for (const base::Value& item_val : pref_list) {
    const base::Value::Dict& item = item_val.GetDict();
    const base::Value::List* urls_list = item.FindList(kPrefUrlsKey);
    std::set<url::Origin> parsed_set;

    // A urls item can contain a pair of URLs that are delimited by a comma. If
    // it does not contain a second URL, set the embedding URL to an empty GURL
    // to signify a wildcard embedded URL.
    for (const auto& urls_value : CHECK_DEREF(urls_list)) {
      std::vector<std::string> urls =
          base::SplitString(urls_value.GetString(), ",", base::TRIM_WHITESPACE,
                            base::SPLIT_WANT_ALL);

      // Skip invalid URL entries.
      if (urls.empty())
        continue;

      auto requesting_origin = url::Origin::Create(GURL(urls[0]));
      std::optional<url::Origin> embedding_origin;
      if (urls.size() == 2 && !urls[1].empty())
        embedding_origin = url::Origin::Create(GURL(urls[1]));

      // In order to be compatible with legacy (requesting,embedding) entries
      // without breaking any access specified, we will grant the permission to
      // the embedder if present because under permission delegation the
      // top-level origin has the permission. If only the requesting origin is
      // present, use that instead.
      auto origin = embedding_origin.has_value() ? embedding_origin.value()
                                                 : requesting_origin;

      parsed_set.insert(std::move(origin));
    }

    // Ignore items with empty parsed URLs.
    if (parsed_set.empty())
      continue;

    // For each device entry in the map, create or update its respective URL
    // set.
    const base::Value::List* devices = item.FindList(kPrefDevicesKey);
    for (const base::Value& device_val : CHECK_DEREF(devices)) {
      const base::Value::Dict& device = device_val.GetDict();
      // A missing ID signifies a wildcard for that ID, so a sentinel value of
      // -1 is assigned.
      const std::optional<int> vendor_id_optional =
          device.FindInt(kPrefVendorIdKey);
      const std::optional<int> product_id_optional =
          device.FindInt(kPrefProductIdKey);
      int vendor_id = vendor_id_optional.value_or(-1);
      int product_id = product_id_optional.value_or(-1);
      DCHECK(vendor_id != -1 || product_id == -1);

      auto key = std::make_pair(vendor_id, product_id);
      usb_device_ids_to_urls_[key].insert(parsed_set.begin(), parsed_set.end());
    }
  }
}
