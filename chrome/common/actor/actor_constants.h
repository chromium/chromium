// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACTOR_ACTOR_CONSTANTS_H_
#define CHROME_COMMON_ACTOR_ACTOR_CONSTANTS_H_

#include <cstddef>

#include "base/time/time.h"

namespace actor {

// 0 is not a valid DOMNodeId so it is used to indicate targeting the
// root/viewport.
inline constexpr int kRootElementDomNodeId = 0;

// How long an ExecuteJs action may run by default, and at most, including
// waiting for a promise it returns. Script runs on the renderer main thread, so
// the limit also bounds how long a script can block the page.
inline constexpr base::TimeDelta kExecuteJsDefaultTimeout = base::Seconds(20);
inline constexpr base::TimeDelta kExecuteJsMaxTimeout = base::Seconds(60);

// Maximum size, in bytes, of an ExecuteJs result. Longer results are truncated.
inline constexpr size_t kExecuteJsMaxResultBytes = 1024 * 1024;

// Maximum number of bytes of an ExecuteJs script included in logs and journal
// entries.
inline constexpr size_t kExecuteJsMaxLoggedScriptBytes = 1024 * 1024;

}  // namespace actor

#endif  // CHROME_COMMON_ACTOR_ACTOR_CONSTANTS_H_
