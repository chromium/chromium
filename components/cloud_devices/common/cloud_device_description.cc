// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cloud_devices/common/cloud_device_description.h"

#include <string_view>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/cloud_devices/common/cloud_device_description_consts.h"

namespace cloud_devices {

namespace {

bool IsValidTicket(const base::Value::Dict& value) {
  const std::string* version = value.FindString(json::kVersion);
  return version && *version == json::kVersion10;
}

}  // namespace

CloudDeviceDescription::CloudDeviceDescription() {
  root_.Set(json::kVersion, base::Value(json::kVersion10));
}

CloudDeviceDescription::~CloudDeviceDescription() = default;

bool CloudDeviceDescription::InitFromString(const std::string& json) {
  std::optional<base::Value> value = base::JSONReader::Read(json);
  if (!value || !value->is_dict()) {
    return false;
  }

  return InitFromValue(std::move(*value).TakeDict());
}

bool CloudDeviceDescription::InitFromValue(base::Value::Dict ticket) {
  root_ = std::move(ticket);
  return IsValidTicket(root_);
}

std::string CloudDeviceDescription::ToStringForTesting() const {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      root_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

base::Value CloudDeviceDescription::ToValue() && {
  return base::Value(std::move(root_));
}

const base::Value::Dict* CloudDeviceDescription::GetDictItem(
    std::string_view path) const {
  return root_.FindDictByDottedPath(path);
}

const base::Value::List* CloudDeviceDescription::GetListItem(
    std::string_view path) const {
  return root_.FindListByDottedPath(path);
}

bool CloudDeviceDescription::SetDictItem(std::string_view path,
                                         base::Value::Dict dict) {
  return root_.SetByDottedPath(path, std::move(dict));
}

bool CloudDeviceDescription::SetListItem(std::string_view path,
                                         base::Value::List list) {
  return root_.SetByDottedPath(path, std::move(list));
}

}  // namespace cloud_devices
