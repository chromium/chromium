// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_TEXT_LOG_RECEIVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_TEXT_LOG_RECEIVER_H_

#include "components/autofill/core/browser/logging/log_receiver.h"

namespace autofill {

// This is a poor man's LogReceiver which logs events to LOG(INFO) to visualize
// log events. It is not fancy enough to render nested tables in a pretty way
// but probably fancy enough to generate some useful debugging signals.
//
// If a test is using the TestAutofillClient, using this TextLogReceiver can
// be enabled via the Finch feature `kAutofillLoggingToTerminal` or
// `kPasswordManagerLoggingToTerminal`.
class TextLogReceiver : public LogReceiver {
 public:
  TextLogReceiver() = default;
  TextLogReceiver(const TextLogReceiver&) = delete;
  TextLogReceiver& operator=(const TextLogReceiver&) = delete;
  ~TextLogReceiver() override = default;

  void LogEntry(const base::Value::Dict& entry) override;

  // Converts a log entry that is passed to the LogEntry() function to text.
  // The logic is extracted into a separate function to enable unit testing.
  std::string LogEntryToText(const base::Value::Dict& entry) const;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_TEXT_LOG_RECEIVER_H_
