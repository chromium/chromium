// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USB_USB_BLOCKLIST_H_
#define CHROME_BROWSER_USB_USB_BLOCKLIST_H_

#include <stdint.h>

#include <vector>

#include "base/lazy_instance.h"

namespace device {
namespace mojom {
class UsbDeviceInfo;
}
}

class UsbBlocklist final {
 public:
  // An entry in the blocklist. Represents a device that should not be
  // accessible using WebUSB.
  struct Entry {
    // Matched against the idVendor field of the USB Device Descriptor.
    uint16_t vendor_id;

    // Matched against the idProduct field of the USB Device Descriptor.
    uint16_t product_id;

    // Compared against the bcdDevice field of the USB Device Descriptor. Any
    // value less than or equal to this will be considered a match.
    uint16_t max_version;
  };

  UsbBlocklist(const UsbBlocklist&) = delete;
  UsbBlocklist& operator=(const UsbBlocklist&) = delete;

  ~UsbBlocklist();

  // Returns a singleton instance of the blocklist.
  static UsbBlocklist& Get();

  // Returns if a device is excluded from access.
  bool IsExcluded(const Entry& entry) const;
  bool IsExcluded(const device::mojom::UsbDeviceInfo& device_info) const;

  // Size of the blocklist.
  size_t GetDynamicEntryCountForTest() const { return dynamic_entries_.size(); }

  // Reload the blocklist for testing purposes.
  void ResetToDefaultValuesForTest();

 private:
  // friend LazyInstance to permit access to private constructor.
  friend base::LazyInstanceTraitsBase<UsbBlocklist>;

  UsbBlocklist();

  // Populates the blocklist with values set via a Finch experiment which allows
  // the set of blocked devices to be updated without shipping new executable
  // versions.
  //
  // The variation string must be a comma-separated list of
  // vendor_id:product_id:max_version triples, where each member of the triple
  // is a 16-bit integer written as exactly 4 hexadecimal digits. The triples
  // may be separated by whitespace. Triple components are colon-separated and
  // must not have whitespace around the colon.
  //
  // Invalid entries in the comma-separated list will be ignored.
  //
  // Example:
  //   "1000:001C:0100, 1000:001D:0101, 123:ignored:0"
  void PopulateWithServerProvidedValues();

  // Set of blocklist entries.
  std::vector<Entry> dynamic_entries_;
};

#endif  // CHROME_BROWSER_USB_USB_BLOCKLIST_H_
