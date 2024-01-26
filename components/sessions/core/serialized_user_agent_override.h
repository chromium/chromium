// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SERIALIZED_USER_AGENT_OVERRIDE_H_
#define COMPONENTS_SESSIONS_CORE_SERIALIZED_USER_AGENT_OVERRIDE_H_

#include <optional>
#include <string>

#include "components/sessions/core/sessions_export.h"

namespace sessions {

// SerializedUserAgentOverride is used to record custom values of user-agent
// and possibly user agent client hints for session and tab restore.
//
// See also blink::UserAgentOverride which this is a dependency-breaking
// representation for.
struct SESSIONS_EXPORT SerializedUserAgentOverride {
  SerializedUserAgentOverride();
  SerializedUserAgentOverride(const SerializedUserAgentOverride& other);
  SerializedUserAgentOverride(SerializedUserAgentOverride&& other);

  SerializedUserAgentOverride& operator=(
      const SerializedUserAgentOverride& other);
  SerializedUserAgentOverride& operator=(SerializedUserAgentOverride&& other);

  ~SerializedUserAgentOverride();

  static SerializedUserAgentOverride UserAgentOnly(const std::string& ua) {
    // Helper which sets only UA, no client hints.
    SerializedUserAgentOverride result;
    result.ua_string_override = ua;
    return result;
  }

  size_t EstimateMemoryUsage() const;

  // Override of User-Agent string. Empty means no override. If non-empty, this
  // string is used as the user agent whenever the tab's NavigationEntries need
  // it overridden.
  std::string ua_string_override;

  // Override of user-agent client hints. Format is user dependent; content/
  // encodes it via blink::UserAgentMetadata::Marshal(). Should be nullopt
  // if |ua_string_override| is empty.
  std::optional<std::string> opaque_ua_metadata_override;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_SERIALIZED_USER_AGENT_OVERRIDE_H_
