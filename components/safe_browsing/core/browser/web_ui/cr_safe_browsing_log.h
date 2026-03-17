// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_CR_SAFE_BROWSING_LOG_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_CR_SAFE_BROWSING_LOG_H_

#include <sstream>

#include "base/logging.h"

namespace safe_browsing {
class WebUIInfoSingleton;

// Used for streaming messages to the WebUIInfoSingleton. Collects
// streamed messages, then sends them to the WebUIInfoSingleton when
// destroyed. Intended to be used in CRSBLOG macro.
class CrSBLogMessage {
 public:
  CrSBLogMessage();
  virtual ~CrSBLogMessage();

  std::ostream& stream() { return stream_; }

 protected:
  // Logs the stream to the WebUIInfoSingleton. This is intended to be used in
  // the child's destructor.
  void LogStreamToInfoSingleton(WebUIInfoSingleton* instance);

 private:
  std::ostringstream stream_;
};

// Used to consume a stream so that we don't even evaluate the streamed data if
// there are no chrome://safe-browsing tabs open.
class CrSBLogVoidify {
 public:
  CrSBLogVoidify() = default;

  // This has to be an operator with a precedence lower than <<,
  // but higher than ?:
  void operator&(std::ostream&) {}
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_CR_SAFE_BROWSING_LOG_H_
