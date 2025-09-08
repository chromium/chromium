// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACTOR_JOURNAL_DETAILS_BUILDER_H_
#define CHROME_COMMON_ACTOR_JOURNAL_DETAILS_BUILDER_H_

#include "base/strings/to_string.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

class JournalDetailsBuilder {
 public:
  JournalDetailsBuilder();
  JournalDetailsBuilder(JournalDetailsBuilder&&);
  ~JournalDetailsBuilder();

  template <typename ValueType>
    requires(requires(const ValueType& value) { base::ToString(value); })
  JournalDetailsBuilder Add(std::string_view key, const ValueType& value) && {
    details_.push_back(
        mojom::JournalDetails::New(std::string(key), base::ToString(value)));
    return std::move(*this);
  }

  template <typename ValueType>
    requires(requires(const ValueType& value) { base::ToString(value); })
  JournalDetailsBuilder AddError(const ValueType& value) && {
    details_.push_back(
        mojom::JournalDetails::New("error", base::ToString(value)));
    return std::move(*this);
  }

  std::vector<mojom::JournalDetailsPtr> Build() && {
    return std::move(details_);
  }

 private:
  std::vector<mojom::JournalDetailsPtr> details_;
};

}  // namespace actor

inline std::ostream& operator<<(
    std::ostream& stream,
    const std::vector<actor::mojom::JournalDetailsPtr>& details) {
  for (const auto& detail : details) {
    stream << detail->key << "=" << detail->value << " ";
  }
  return stream;
}

#endif  // CHROME_COMMON_ACTOR_JOURNAL_DETAILS_BUILDER_H_
