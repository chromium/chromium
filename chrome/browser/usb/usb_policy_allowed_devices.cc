// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_policy_allowed_devices.h"

#include "base/values.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "device/usb/public/mojom/device_manager.mojom.h"
#include "url/gurl.h"

namespace {

constexpr char kPrefDevicesKey[] = "devices";
constexpr char kPrefUrlPatternsKey[] = "url_patterns";
constexpr char kPrefVendorIdKey[] = "vendor_id";
constexpr char kPrefProductIdKey[] = "product_id";

// Find the URL match by checking if the pattern matches the given GURL types
// using ContentSettingsPattern::Matches().
bool FindMatchInSet(const std::set<content_settings::PatternPair>& pattern_set,
                    const GURL& requesting_origin,
                    const GURL& embedding_origin) {
  for (const auto& pattern : pattern_set) {
    if (pattern.first.Matches(requesting_origin) &&
        pattern.second.Matches(embedding_origin)) {
      return true;
    }
  }
  return false;
}

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
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const device::mojom::UsbDeviceInfo& device_info) {
  // Search through each set of URL patterns that match the given device. The
  // keys correspond to the following URL pattern sets:
  //  * (vendor_id, product_id): A set corresponding to the exact device.
  //  * (vendor_id, -1): A set corresponding to any device with |vendor_id|.
  //  * (-1, -1): A set corresponding to any device.
  const std::pair<int, int> set_keys[] = {
      std::make_pair(device_info.vendor_id, device_info.product_id),
      std::make_pair(device_info.vendor_id, -1), std::make_pair(-1, -1)};

  for (const auto& key : set_keys) {
    const auto patterns = usb_device_ids_to_url_patterns_.find(key);
    if (patterns == usb_device_ids_to_url_patterns_.cend())
      continue;

    if (FindMatchInSet(patterns->second, requesting_origin, embedding_origin))
      return true;
  }
  return false;
}

void UsbPolicyAllowedDevices::CreateOrUpdateMap() {
  const base::Value* pref_value = pref_change_registrar_.prefs()->Get(
      prefs::kManagedWebUsbAllowDevicesForUrls);
  usb_device_ids_to_url_patterns_.clear();

  // A policy has not been assigned.
  if (!pref_value) {
    return;
  }

  // The pref value has already been validated by the policy handler, so it is
  // safe to assume that |pref_value| follows the policy template.
  for (const auto& item : pref_value->GetList()) {
    const base::Value* url_patterns = item.FindKey(kPrefUrlPatternsKey);
    std::set<content_settings::PatternPair> parsed_url_set;

    // Parse each URL pattern into a PatternPair and store it in
    // |parsed_url_set|.
    for (const auto& url_pattern : url_patterns->GetList()) {
      content_settings::PatternPair pattern_pair =
          content_settings::ParsePatternString(url_pattern.GetString());

      // Ignore invalid patterns.
      if (!pattern_pair.first.IsValid())
        continue;

      parsed_url_set.insert(std::move(pattern_pair));
    }

    // Ignore items with empty parsed URLs.
    if (parsed_url_set.empty())
      continue;

    // For each device entry in the map, create or update its respective URL
    // pattern set.
    const base::Value* devices = item.FindKey(kPrefDevicesKey);
    for (const auto& device : devices->GetList()) {
      // A missing ID signifies a wildcard for that ID, so a sentinel value of
      // -1 is assigned.
      const base::Value* vendor_id_value = device.FindKey(kPrefVendorIdKey);
      const base::Value* product_id_value = device.FindKey(kPrefProductIdKey);
      int vendor_id = vendor_id_value ? vendor_id_value->GetInt() : -1;
      int product_id = product_id_value ? product_id_value->GetInt() : -1;
      DCHECK(vendor_id != -1 || product_id == -1);

      auto key = std::make_pair(vendor_id, product_id);
      usb_device_ids_to_url_patterns_[key].insert(parsed_url_set.begin(),
                                                  parsed_url_set.end());
    }
  }
}
