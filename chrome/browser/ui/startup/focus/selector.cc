// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/focus/selector.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace focus {

Selector::Selector(SelectorType type, const std::string& app_id)
    : type(type), app_id(app_id) {}

Selector::Selector(SelectorType type, const GURL& url) : type(type), url(url) {}

Selector::~Selector() = default;

Selector::Selector(const Selector& other) = default;
Selector& Selector::operator=(const Selector& other) = default;
Selector::Selector(Selector&& other) noexcept = default;
Selector& Selector::operator=(Selector&& other) noexcept = default;

bool Selector::IsValid() const {
  switch (type) {
    case SelectorType::kApp:
      return !app_id.empty();
    case SelectorType::kUrlExact:
    case SelectorType::kUrlPrefix:
      return url.is_valid();
  }
  return false;
}

std::string Selector::ToString() const {
  switch (type) {
    case SelectorType::kApp:
      return "app:" + app_id;
    case SelectorType::kUrlExact:
      return url.spec();
    case SelectorType::kUrlPrefix:
      return url.spec() + "*";
  }
  return "";
}

std::vector<Selector> ParseSelectors(const std::string& input) {
  std::vector<Selector> selectors;

  if (input.empty()) {
    return selectors;
  }

  std::vector<std::string_view> items = base::SplitStringPiece(
      input, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (const auto& item : items) {
    if (base::StartsWith(item, "app:", base::CompareCase::SENSITIVE)) {
      std::string_view app_id = item.substr(4);
      if (!app_id.empty()) {
        selectors.emplace_back(SelectorType::kApp, std::string(app_id));
      }
    } else {
      bool is_prefix = base::EndsWith(item, "*", base::CompareCase::SENSITIVE);
      std::string_view url_part = item;
      if (is_prefix) {
        url_part = item.substr(0, item.length() - 1);
      }

      GURL url(url_part);
      if (url.is_valid() && (url.has_host() || url.has_path())) {
        SelectorType type =
            is_prefix ? SelectorType::kUrlPrefix : SelectorType::kUrlExact;
        selectors.emplace_back(type, url);
      }
    }
  }

  return selectors;
}

}  // namespace focus
