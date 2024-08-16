// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/network/network_ui_data.h"

#include <utility>

#include "base/check.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace ash {

namespace {

const char kKeyONCSource[] = "onc_source";
const char kKeyUserSettings[] = "user_settings";
const char kONCSourceUserImport[] = "user_import";
const char kONCSourceDevicePolicy[] = "device_policy";
const char kONCSourceUserPolicy[] = "user_policy";

template <typename Enum>
struct StringEnumEntry {
  const char* string;
  Enum enum_value;
};

const StringEnumEntry<::onc::ONCSource> kONCSourceTable[] = {
    {kONCSourceUserImport, ::onc::ONC_SOURCE_USER_IMPORT},
    {kONCSourceDevicePolicy, ::onc::ONC_SOURCE_DEVICE_POLICY},
    {kONCSourceUserPolicy, ::onc::ONC_SOURCE_USER_POLICY}};

// Converts |enum_value| to the corresponding string according to |table|. If no
// enum value of the table matches (which can only occur if incorrect casting
// was used to obtain |enum_value|), returns an empty string instead.
template <typename Enum, int N>
std::string EnumToString(const StringEnumEntry<Enum> (&table)[N],
                         Enum enum_value) {
  for (int i = 0; i < N; ++i) {
    if (table[i].enum_value == enum_value)
      return table[i].string;
  }
  return std::string();
}

// Converts |str| to the corresponding enum value according to |table|. If no
// string of the table matches, returns |fallback| instead.
template <typename Enum, int N>
Enum StringToEnum(const StringEnumEntry<Enum> (&table)[N],
                  const std::string& str,
                  Enum fallback) {
  for (int i = 0; i < N; ++i) {
    if (table[i].string == str)
      return table[i].enum_value;
  }
  return fallback;
}

}  // namespace

NetworkUIData::NetworkUIData() : onc_source_(::onc::ONC_SOURCE_NONE) {}

NetworkUIData::NetworkUIData(const NetworkUIData& other) {
  *this = other;
}

NetworkUIData& NetworkUIData::operator=(const NetworkUIData& other) {
  onc_source_ = other.onc_source_;
  if (other.user_settings_.has_value()) {
    user_settings_ = other.user_settings_->Clone();
  }
  return *this;
}

NetworkUIData::NetworkUIData(const base::Value::Dict& dict) {
  const std::string* source_value = dict.FindString(kKeyONCSource);
  if (source_value) {
    onc_source_ =
        StringToEnum(kONCSourceTable, *source_value, ::onc::ONC_SOURCE_NONE);
  } else {
    onc_source_ = ::onc::ONC_SOURCE_NONE;
  }

  const base::Value::Dict* user_settings_value =
      dict.FindDict(kKeyUserSettings);
  if (user_settings_value) {
    user_settings_ = user_settings_value->Clone();
  }
}

NetworkUIData::~NetworkUIData() = default;

// static
std::unique_ptr<NetworkUIData> NetworkUIData::CreateFromONC(
    ::onc::ONCSource onc_source) {
  std::unique_ptr<NetworkUIData> ui_data(new NetworkUIData());

  ui_data->onc_source_ = onc_source;

  return ui_data;
}

const base::Value::Dict* NetworkUIData::GetUserSettingsDictionary() const {
  if (!user_settings_.has_value()) {
    return nullptr;
  }
  return &user_settings_.value();
}

void NetworkUIData::SetUserSettingsDictionary(base::Value::Dict dict) {
  user_settings_ = std::move(dict);
}

std::string NetworkUIData::GetAsJson() const {
  base::Value::Dict dict;
  const std::string source_string = GetONCSourceAsString();
  if (!source_string.empty()) {
    dict.Set(kKeyONCSource, source_string);
  }
  if (user_settings_.has_value()) {
    dict.Set(kKeyUserSettings, user_settings_->Clone());
  }

  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

std::string NetworkUIData::GetONCSourceAsString() const {
  return EnumToString(kONCSourceTable, onc_source_);
}

}  // namespace ash
