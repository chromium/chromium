// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simple test helper to repeatedly emit a string to stdout, stderr, or both.

// Usage:
// emit_text [--text=<string>] [--count=<loops>] [--stdout] [--stderr]
// Prints `loops` copies of `string` to stdout and/or stderr.
//
// To emit no output efficiently, set count to 0. To emit no output
// inefficiently, set text to the empty string. By default, this assumes a
// count of 1, text of "text", and if neither --stdout nor --stderr is
// specified, stdout. (If --stderr is specified, --stdout is not inferred.
// These options do not conflict; explicitly specify both to duplicate output
// to both streams, starting with stdout, flushing after each loop.)
//
// Return values:
// 0 -- successful exit
// ERANGE (34) -- <count> out of range for uint64 or not a parseable number
// any other value -- there is a bug in emit_text
//
// The ERANGE error behavior can be relied on by tests that want to verify
// behaviors when a launched process returns nonzero. (Other errors might
// change if this helper needs more features for some reason.)

#include <cerrno>
#include <cstdint>
#include <iostream>
#include <string>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"

int main(int argc, char** argv) {
  if (!base::CommandLine::Init(argc, argv)) {
    return EALREADY;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  bool do_stderr = command_line->HasSwitch("stderr");
  bool do_stdout = !do_stderr || command_line->HasSwitch("stdout");

  uint64_t count = 1;
  if (command_line->HasSwitch("count")) {
    if (!base::StringToUint64(command_line->GetSwitchValueASCII("count"),
                              &count)) {
      return ERANGE;
    }
  }

  std::string text = command_line->HasSwitch("text")
                         ? command_line->GetSwitchValueASCII("text")
                         : "text";

  for (uint64_t i = 0; i < count; i++) {
    if (do_stdout) {
      std::cout << text << std::flush;
    }
    if (do_stderr) {
      std::cerr << text << std::flush;
    }
  }

  return 0;
}
