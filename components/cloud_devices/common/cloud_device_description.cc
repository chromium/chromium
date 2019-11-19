// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cloud_devices/common/cloud_device_description.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/cloud_devices/common/cloud_device_description_consts.h"

namespace cloud_devices {

CloudDeviceDescription::CloudDeviceDescription()
    : root_(base::Value(base::Value::Type::DICTIONARY)) {
  root_.SetKey(json::kVersion, base::Value(json::kVersion10));
}

CloudDeviceDescription::~CloudDeviceDescription() = default;

bool CloudDeviceDescription::InitFromString(const std::string& json) {
  base::Optional<base::Value> value = base::JSONReader::Read(json);
  if (!value)
    return false;

  return InitFromValue(std::move(*value));
}

bool CloudDeviceDescription::InitFromValue(base::Value ticket) {
  if (!ticket.is_dict())
    return false;
  root_ = std::move(ticket);
  return IsValidTicket(root_);
}

// static
bool CloudDeviceDescription::IsValidTicket(const base::Value& ticket) {
  if (!ticket.is_dict())
    return false;

  const base::Value* version = ticket.FindKey(json::kVersion);
  return version && version->GetString() == json::kVersion10;
}

std::string CloudDeviceDescription::ToString() const {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      root_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

base::Value CloudDeviceDescription::ToValue() && {
  return std::move(root_);
}

const base::Value* CloudDeviceDescription::GetItem(
    const std::vector<base::StringPiece>& path,
    base::Value::Type type) const {
  return root_.FindPathOfType(path, type);
}

base::Value* CloudDeviceDescription::CreateItem(
    const std::vector<base::StringPiece>& path,
    base::Value::Type type) {
  return root_.SetPath(path, base::Value(type));
}

}  // namespace cloud_devices
