// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/focus/selector.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "url/gurl.h"

namespace focus {

Selector::Selector(SelectorType type, GURL url)
    : type(type), url(std::move(url)) {}

Selector::~Selector() = default;

Selector::Selector(const Selector& other) = default;
Selector& Selector::operator=(const Selector& other) = default;
Selector::Selector(Selector&& other) noexcept = default;
Selector& Selector::operator=(Selector&& other) noexcept = default;

bool Selector::IsValid() const {
  return url.is_valid();
}

std::string Selector::ToString() const {
  if (type == SelectorType::kUrlPrefix) {
    return base::StrCat({url.spec(), "*"});
  }
  return url.spec();
}

std::vector<Selector> ParseSelectors(std::string_view input) {
  std::vector<Selector> selectors;

  if (input.empty()) {
    return selectors;
  }

  for (std::string_view item : base::SplitStringPiece(
           input, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    std::optional<std::string_view> trimmed = base::RemoveSuffix(item, "*");
    std::string_view url_part = trimmed.value_or(item);

    GURL url(url_part);
    if (url.is_valid() && (url.has_host() || url.has_path())) {
      SelectorType type = trimmed.has_value() ? SelectorType::kUrlPrefix
                                              : SelectorType::kUrlExact;
      selectors.emplace_back(type, std::move(url));
    }
  }

  return selectors;
}

}  // namespace focus
