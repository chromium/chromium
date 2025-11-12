// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACTOR_JOURNAL_DETAILS_BUILDER_H_
#define CHROME_COMMON_ACTOR_JOURNAL_DETAILS_BUILDER_H_

#include "base/strings/to_string.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/task_id.h"

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
  JournalDetailsBuilder& Add(std::string_view key, const ValueType& value) & {
    details_.push_back(
        mojom::JournalDetails::New(std::string(key), base::ToString(value)));
    return *this;
  }

  template <typename ValueType>
    requires(requires(const ValueType& value) { base::ToString(value); })
  JournalDetailsBuilder AddError(const ValueType& value) && {
    details_.push_back(
        mojom::JournalDetails::New("error", base::ToString(value)));
    return std::move(*this);
  }

  template <typename ValueType>
    requires(requires(const ValueType& value) { base::ToString(value); })
  JournalDetailsBuilder& AddError(const ValueType& value) & {
    details_.push_back(
        mojom::JournalDetails::New("error", base::ToString(value)));
    return *this;
  }

  std::vector<mojom::JournalDetailsPtr> Build() && {
    return std::move(details_);
  }

 private:
  std::vector<mojom::JournalDetailsPtr> details_;
};

// The default global track.
inline constexpr uint64_t kGlobalTrackUUID = 0;

// A specific browser track for a task.
inline uint64_t MakeBrowserTrackUUID(TaskId task_id) {
  constexpr uint64_t kBrowserTrack = 0xda00000000000000LL;
  return kBrowserTrack + task_id.value();
}

// A specific renderer track for a task.
inline uint64_t MakeRendererTrackUUID(TaskId task_id) {
  constexpr uint64_t kRendererTrack = 0xda00000100000000LL;
  return kRendererTrack + task_id.value();
}

// A specific front end track for a task.
inline uint64_t MakeFrontEndTrackUUID(TaskId task_id) {
  constexpr uint64_t kFrontEndTrack = 0xda00000200000000LL;
  return kFrontEndTrack + task_id.value();
}

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
