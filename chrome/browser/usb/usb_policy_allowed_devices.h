// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_POLICY_ALLOWED_DEVICES_H_
#define CHROME_BROWSER_USB_USB_POLICY_ALLOWED_DEVICES_H_

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/prefs/pref_change_registrar.h"

namespace device {
namespace mojom {
class UsbDeviceInfo;
}  // namespace mojom
}  // namespace device

class GURL;
class PrefService;

// This class is used to initialize a UsbDeviceIdsToUrlPatternsMap from the
// preference value for the WebUsbAllowDevicesForUrls policy. The map
// provides an efficient method of checking if a particular device is allowed to
// be used by the given requesting and embedding origins. Additionally, this
// class also uses |pref_change_registrar_| to observe for changes to the
// preference value so that the map can be updated accordingly.
class UsbPolicyAllowedDevices {
 public:
  // A map of device IDs to a set of parsed URLs stored in a
  // content_settings::PatternPair. The device IDs correspond to a pair of
  // |vendor_id| and |product_id|. The content_settings::PatternPair is simply
  // an alias for a pair of content_settings::ContentSettingsPattern objects.
  using UsbDeviceIdsToUrlPatternsMap =
      std::map<std::pair<int, int>, std::set<content_settings::PatternPair>>;

  // Initializes |pref_change_registrar_| with |pref_service| and adds an
  // an observer for the pref path |kManagedWebUsbAllowDevicesForUrls|.
  explicit UsbPolicyAllowedDevices(PrefService* pref_service);
  ~UsbPolicyAllowedDevices();

  // Checks if |requesting_origin| (when embedded within |embedding_origin|) is
  // allowed to use the device with |device_info|.
  bool IsDeviceAllowed(const GURL& requesting_origin,
                       const GURL& embedding_origin,
                       const device::mojom::UsbDeviceInfo& device_info);

  const UsbDeviceIdsToUrlPatternsMap& map() const {
    return usb_device_ids_to_url_patterns_;
  }

 private:
  // Creates or updates the |usb_device_ids_to_url_patterns_| map using the
  // pref at the path |kManagedWebUsbAllowDevicesForUrls|. The existing map is
  // cleared to ensure that previous pref settings are removed.
  void CreateOrUpdateMap();

  // Allow for this class to observe changes to the pref value.
  PrefChangeRegistrar pref_change_registrar_;
  UsbDeviceIdsToUrlPatternsMap usb_device_ids_to_url_patterns_;
};

#endif  // CHROME_BROWSER_USB_USB_POLICY_ALLOWED_DEVICES_H_
