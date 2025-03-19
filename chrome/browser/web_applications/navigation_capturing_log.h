// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_LOG_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_LOG_H_

#include <cstdint>
#include <list>
#include <optional>
#include <string_view>

namespace base {
class Value;
}

namespace web_app {

// Stores debug information surfaced in chrome://web-app-internals and printed
// in failing tests.
class NavigationCapturingLog {
 public:
  NavigationCapturingLog();
  ~NavigationCapturingLog();

  void LogData(std::string_view source,
               base::Value value,
               std::optional<int64_t> navigation_handle_id);

  // This cannot be used for any production logic.
  base::Value GetLog() const;

 private:
  std::list<base::Value> debug_log_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_LOG_H_
