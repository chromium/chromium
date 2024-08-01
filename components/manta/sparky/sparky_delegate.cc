// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/sparky/sparky_delegate.h"

#include <memory>
#include <optional>

#include "base/values.h"

namespace manta {

SettingsData::SettingsData(const std::string& pref_name,
                           const PrefType& pref_type,
                           std::optional<base::Value> value)
    : pref_name(pref_name), pref_type(pref_type) {
  switch (pref_type) {
    case PrefType::kBoolean:
      bool_val = value->GetBool();
      val_set = true;
      break;
    case PrefType::kInt:
      int_val = value->GetInt();
      val_set = true;
      break;
    case PrefType::kDouble:
      double_val = value->GetDouble();
      val_set = true;
      break;
    case PrefType::kString:
      string_val = value->GetString();
      val_set = true;
      break;
    default:
      // TODO add in error message.
      break;
  }
}

SettingsData::~SettingsData() = default;

SettingsData::SettingsData(const SettingsData&) = default;
SettingsData& SettingsData::operator=(const SettingsData&) = default;

std::optional<base::Value> SettingsData::GetValue() const {
  if (!val_set) {
    return std::nullopt;
  }
  switch (pref_type) {
    case PrefType::kBoolean:
      return std::make_optional<base::Value>(bool_val);
    case PrefType::kInt:
      return std::make_optional<base::Value>(int_val);
    case PrefType::kDouble:
      return std::make_optional<base::Value>(double_val);
    case PrefType::kString:
      return std::make_optional<base::Value>(string_val);
    default:
      return std::nullopt;
  }
}

AppsData::AppsData(const std::string& name, const std::string& id)
    : name(name), id(id) {}

AppsData::~AppsData() = default;

AppsData::AppsData(AppsData&& other) = default;
AppsData& AppsData::operator=(AppsData&& other) = default;

void AppsData::AddSearchableText(const std::string& new_searchable_text) {
  searchable_text.push_back(new_searchable_text);
}
FileData::FileData(const std::string& path,
                   const std::string& name,
                   const std::string& date_modified)
    : path(path), name(name), date_modified(date_modified) {}

FileData::~FileData() = default;

FileData::FileData(const FileData&) = default;
FileData& FileData::operator=(const FileData&) = default;

SparkyDelegate::SparkyDelegate() = default;
SparkyDelegate::~SparkyDelegate() = default;

}  // namespace manta
