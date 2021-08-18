// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_STATUS_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_STATUS_H_

#include <string>

// WebDriver standard status codes.
enum StatusCode {
  kOk = 0,
  kInvalidSessionId = 6,
  kNoSuchElement = 7,
  kNoSuchFrame = 8,
  kUnknownCommand = 9,
  kStaleElementReference = 10,
  kElementNotVisible = 11,
  kInvalidElementState = 12,
  kUnknownError = 13,
  kJavaScriptError = 17,
  kXPathLookupError = 19,
  kTimeout = 21,
  kNoSuchWindow = 23,
  kInvalidCookieDomain = 24,
  kUnableToSetCookie = 25,
  kUnexpectedAlertOpen = 26,
  kNoSuchAlert = 27,
  kScriptTimeout = 28,
  kInvalidSelector = 32,
  kSessionNotCreated = 33,
  kMoveTargetOutOfBounds = 34,
  kElementNotInteractable = 60,
  kInvalidArgument = 61,
  kNoSuchCookie = 62,
  kElementClickIntercepted = 64,
  kUnsupportedOperation = 405,
  // Chrome-specific status codes.
  kChromeNotReachable = 100,
  kNoSuchExecutionContext,
  kDisconnected,
  kForbidden = 103,
  kTabCrashed,
  kTargetDetached,
  kUnexpectedAlertOpen_Keep,
};

// Represents a WebDriver status, which may be an error or ok.
class Status {
 public:
  explicit Status(StatusCode code);
  Status(StatusCode code, const std::string& details);
  Status(StatusCode code, const Status& cause);
  Status(StatusCode code, const std::string& details, const Status& cause);
  ~Status();

  void AddDetails(const std::string& details);

  bool IsOk() const;
  bool IsError() const;

  StatusCode code() const;

  const std::string& message() const;

  const std::string& stack_trace() const;

 private:
  StatusCode code_;
  std::string msg_;
  std::string stack_trace_;
};

// Returns the standard error code string associated with a StatusCode, as
// defined by W3C (https://w3c.github.io/webdriver/#dfn-error-code).
const char* StatusCodeToString(StatusCode code);

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_STATUS_H_
