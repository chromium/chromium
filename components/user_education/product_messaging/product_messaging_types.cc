// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/product_messaging/product_messaging_types.h"

#include <array>
#include <sstream>

#include "base/check.h"

namespace user_education {

bool ProductMessageKey::operator<(ProductMessageKey other) const {
  DCHECK(id_ != other.id_ || type_ == other.type_)
      << "Found two product message keys with same id " << id_
      << " but different types.";
  return id_ < other.id_;
}

std::string ProductMessageKey::GetName() const {
  std::string temp = id_.GetName();
  if (!temp.empty()) {
    CHECK(temp.ends_with(internal::kProductMessageUniqueIdSuffix));
    temp = temp.substr(
        0, temp.length() - internal::kProductMessageUniqueIdSuffix.length());
  }
  return temp;
}

std::string ProductMessageKey::ToString() const {
  static constexpr auto kTypeNames = std::to_array<std::string_view>({
      "[none]",
      "LowPriorityForTesting",
      "AnchoredMessage",
      "LowPriorityIph",
      "HighPriorityIph",
      "LegalOrComplianceNotice",
      "HighPriorityForTesting",
  });
  static_assert(kTypeNames.size() ==
                static_cast<size_t>(ProductMessageType::kMaxValue) + 1U);
  std::ostringstream oss;
  oss << "ProductMessageKey{ type: "
      << kTypeNames.at(static_cast<size_t>(type_)) << " id: " << id_.GetName()
      << " }";
  return oss.str();
}

}  // namespace user_education
