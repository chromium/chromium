// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_FORM_ANALYSER_LOGGER_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_FORM_ANALYSER_LOGGER_H_

#include <string>
#include <vector>

#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"

namespace autofill {

using ConsoleLevel = blink::mojom::ConsoleMessageLevel;

// ConsoleLogger provides a convenient interface for logging messages to the
// DevTools console, both in terms of wrapping and formatting console messages
// along with their parameters, and in ordering messages so that higher-priority
// warnings are displayed first.
class PageFormAnalyserLogger {
 public:
  static const ConsoleLevel kError = blink::mojom::ConsoleMessageLevel::kError;
  static const ConsoleLevel kWarning =
      blink::mojom::ConsoleMessageLevel::kWarning;
  static const ConsoleLevel kVerbose =
      blink::mojom::ConsoleMessageLevel::kVerbose;

  explicit PageFormAnalyserLogger(blink::WebLocalFrame* frame);
  ~PageFormAnalyserLogger();

  // Virtual for testing.
  virtual void Send(std::string message,
                    ConsoleLevel level,
                    blink::WebNode node);
  virtual void Send(std::string message,
                    ConsoleLevel level,
                    std::vector<blink::WebNode> nodes);
  virtual void Flush();

 private:
  blink::WebLocalFrame* frame_;

  // Though PageFormAnalyserLogger provides buffering, it is intended to be
  // simply over the course of a single analysis, for ordering purposes.
  struct LogEntry;
  std::map<ConsoleLevel, std::vector<LogEntry>> node_buffer_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PAGE_FORM_ANALYSER_LOGGER_H_
