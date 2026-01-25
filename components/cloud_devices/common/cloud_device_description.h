// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLOUD_DEVICES_COMMON_CLOUD_DEVICE_DESCRIPTION_H_
#define COMPONENTS_CLOUD_DEVICES_COMMON_CLOUD_DEVICE_DESCRIPTION_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/values.h"

namespace cloud_devices {

// Provides parsing, serialization and validation Cloud Device Description or
// Cloud Job Ticket.
// https://developers.google.com/cloud-print/docs/cdd
class CloudDeviceDescription {
 public:
  CloudDeviceDescription();

  CloudDeviceDescription(const CloudDeviceDescription&) = delete;
  CloudDeviceDescription& operator=(const CloudDeviceDescription&) = delete;

  ~CloudDeviceDescription();

  bool InitFromString(const std::string& json);
  bool InitFromValue(base::DictValue value);

  std::string ToStringForTesting() const;

  base::Value ToValue() &&;

  // Returns item of given type with capability/option.
  // Returns nullptr if missing.
  const base::DictValue* GetDictItem(std::string_view path) const;
  const base::ListValue* GetListItem(std::string_view path) const;

  // Sets item with given type for capability/option. Returns false if an
  // intermediate Value in the path is not a dictionary.
  bool SetDictItem(std::string_view path, base::DictValue dict);
  bool SetListItem(std::string_view path, base::ListValue list);

 private:
  base::DictValue root_;
};

}  // namespace cloud_devices

#endif  // COMPONENTS_CLOUD_DEVICES_COMMON_CLOUD_DEVICE_DESCRIPTION_H_
