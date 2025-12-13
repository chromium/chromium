// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_FOCUS_FOCUS_HANDLER_H_
#define CHROME_BROWSER_UI_STARTUP_FOCUS_FOCUS_HANDLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ui/startup/focus/match_candidate.h"
#include "chrome/browser/ui/startup/focus/selector.h"

class Profile;

namespace base {
class CommandLine;
}  // namespace base

namespace focus {

enum class FocusStatus {
  kFocused,         // Successfully focused an existing tab/window.
  kNoMatch,         // No matching tab/window found for the selectors.
  kParseError,      // Failed to parse the selector string.
  kOpenedFallback,  // Opened a new tab/window with fallback URL.
};

struct FocusResult {
  enum class Error {
    kNone,           // No error occurred.
    kEmptySelector,  // Selector string was empty.
    kInvalidFormat,  // Selector format was invalid.
  };

  explicit FocusResult(FocusStatus status);
  FocusResult(FocusStatus status,
              const std::string& matched_selector,
              const std::string& matched_url);
  FocusResult(FocusStatus status, Error error_type);
  FocusResult(FocusStatus status, std::string opened_url);
  FocusResult(const FocusResult& other);
  FocusResult(FocusResult&& other) noexcept;
  FocusResult& operator=(const FocusResult& other);
  FocusResult& operator=(FocusResult&& other) noexcept;
  ~FocusResult();

  bool IsSuccess() const;
  bool HasMatch() const;

  FocusStatus status;
  std::optional<std::string> matched_selector;
  std::optional<std::string> matched_url;
  std::optional<std::string> opened_url;
  Error error_type;
};

// Main entry point for processing focus requests from command line arguments.
FocusResult ProcessFocusRequest(const base::CommandLine& command_line,
                                Profile& profile);

// Process focus request and write JSON results to file if specified.
FocusResult ProcessFocusRequestWithResultFile(
    const base::CommandLine& command_line,
    Profile& profile);
}  // namespace focus

#endif  // CHROME_BROWSER_UI_STARTUP_FOCUS_FOCUS_HANDLER_H_
