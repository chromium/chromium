// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_key.h"

#include "base/debug/stack_trace.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"

namespace crash_reporter {
namespace internal {

std::string FormatStackTrace(const base::debug::StackTrace& trace,
                             size_t max_length) {
  base::span<const void* const> addresses = trace.addresses();

  std::string value;
  for (const void* address : addresses) {
    std::string address_as_string =
        base::StringPrintf("0x%" PRIx64, reinterpret_cast<uint64_t>(address));
    if (value.size() + address_as_string.size() > max_length) {
      break;
    }
    value += address_as_string;
    value += ' ';
  }

  if (!value.empty() && value.back() == ' ') {
    value.resize(value.size() - 1);
  }

  return value;
}

}  // namespace internal
}  // namespace crash_reporter
