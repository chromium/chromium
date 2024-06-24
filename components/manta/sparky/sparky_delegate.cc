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
    : pref_name(pref_name), pref_type(pref_type), value(std::move(value)) {}

SettingsData::~SettingsData() = default;

void SettingsData::UpdateValue(std::optional<base::Value> new_value) {
  value = std::move(new_value);
}

AppsData::AppsData(const std::string& name, const std::string& id)
    : name(name), id(id) {}

AppsData::~AppsData() = default;

AppsData::AppsData(AppsData&& other) = default;
AppsData& AppsData::operator=(AppsData&& other) = default;

void AppsData::AddSearchableText(const std::string& new_searchable_text) {
  searchable_text.push_back(new_searchable_text);
}

SparkyDelegate::SparkyDelegate() = default;
SparkyDelegate::~SparkyDelegate() = default;

}  // namespace manta
