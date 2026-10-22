// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"

namespace settings_search {

SettingsSearchItem::SettingsSearchItem(mojom::SettingId id,
                                       std::string route_path,
                                       std::u16string title,
                                       std::vector<std::u16string> breadcrumbs,
                                       std::vector<std::u16string> synonyms)
    : id_(id),
      route_path_(std::move(route_path)),
      title_(std::move(title)),
      breadcrumbs_(std::move(breadcrumbs)),
      synonyms_(std::move(synonyms)) {}

SettingsSearchItem::~SettingsSearchItem() = default;

const std::u16string& SettingsSearchItem::GetTitle() const {
  return title_;
}

const std::u16string& SettingsSearchItem::GetSecondaryText() const {
  return breadcrumbs_.empty() ? base::EmptyString16() : breadcrumbs_.back();
}

base::span<const std::u16string> SettingsSearchItem::GetSynonyms() const {
  return synonyms_;
}

}  // namespace settings_search
