// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_CORE_ACTOR_UTIL_H_
#define COMPONENTS_ACTOR_CORE_ACTOR_UTIL_H_

#include <cstddef>
#include <string>

namespace actor {

// Whether actor safety checks are disabled.
bool IsActorSafetyCheckDisabled();

// Whether actor navigation gating is enabled.
bool IsNavigationGatingEnabled();

// Truncates `str` (which must already satisfy
// `base::IsStringUTF8AllowingNoncharacters()`) to at most `max_bytes` bytes
// without splitting a multi-byte UTF-8 sequence. Unlike
// `base::TruncateUTF8ToByteSize()`, this preserves trailing Unicode
// noncharacters (such as U+FFFF).
void TruncateValidUtf8ToByteSize(std::string& str, size_t max_bytes);

}  // namespace actor

#endif  // COMPONENTS_ACTOR_CORE_ACTOR_UTIL_H_
