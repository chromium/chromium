// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_WAYLAND_COMPATIBILITY_TEST_WAYLAND_CLIENT_EVENT_RECORDER_H_
#define COMPONENTS_EXO_WAYLAND_COMPATIBILITY_TEST_WAYLAND_CLIENT_EVENT_RECORDER_H_

#include <cstdint>
#include <string>
#include <unordered_map>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace exo {
namespace wayland {
namespace compatibility {
namespace test {

struct EventRecorder {
  EventRecorder();
  ~EventRecorder();

  void OnEvent(const char* event_name, uint32_t version) noexcept {
    data.emplace(event_name, version);
  }

  absl::optional<uint32_t> MaybeGetReceivedAtVersion(
      const char* event_name) const noexcept {
    auto it = data.find(event_name);
    return it != data.end() ? absl::make_optional(it->second) : absl::nullopt;
  }

  std::unordered_map<std::string, uint32_t> data;
};

}  // namespace test
}  // namespace compatibility
}  // namespace wayland
}  // namespace exo

#endif  // COMPONENTS_EXO_WAYLAND_COMPATIBILITY_TEST_WAYLAND_CLIENT_EVENT_RECORDER_H_
