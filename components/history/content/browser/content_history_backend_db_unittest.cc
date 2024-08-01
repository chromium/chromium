// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// History unit tests come in two flavors:
//
// 1. The more complicated style is that the unit test creates a full history
//    service. This spawns a background thread for the history backend, and
//    all communication is asynchronous. This is useful for testing more
//    complicated things or end-to-end behavior.
//
// 2. The simpler style is to create a history backend on this thread and
//    access it directly without a HistoryService object. This is much simpler
//    because communication is synchronous. Generally, sets should go through
//    the history backend (since there is a lot of logic) but gets can come
//    directly from the HistoryDatabase. This is because the backend generally
//    has no logic in the getter except threading stuff, which we don't want
//    to run.

#include <stddef.h>

#include "components/history/core/browser/history_backend.h"
#include "components/history/core/test/history_backend_db_base_test.h"

namespace history {
namespace {

// This must be outside the anonymous namespace for the friend statement in
// HistoryBackend to work.
class ContentHistoryBackendDBTest : public HistoryBackendDBBaseTest {
 public:
  ContentHistoryBackendDBTest() {}
  ~ContentHistoryBackendDBTest() override {}
};

struct InterruptReasonAssociation {
  std::string name;
  int value;
};

// Test is dependent on interrupt reasons being listed in header file
// in order.
const InterruptReasonAssociation current_reasons[] = {
#define INTERRUPT_REASON(a, b) { #a, b },
#include "components/download/public/common/download_interrupt_reason_values.h"
#undef INTERRUPT_REASON
};

// This represents a list of all reasons we've previously used;
// Do Not Remove Any Entries From This List.
const InterruptReasonAssociation historical_reasons[] = {
    {"FILE_FAILED", 1},
    {"FILE_ACCESS_DENIED", 2},
    {"FILE_NO_SPACE", 3},
    {"FILE_NAME_TOO_LONG", 5},
    {"FILE_TOO_LARGE", 6},
    {"FILE_VIRUS_INFECTED", 7},
    {"FILE_TRANSIENT_ERROR", 10},
    {"FILE_BLOCKED", 11},
    {"FILE_SECURITY_CHECK_FAILED", 12},
    {"FILE_TOO_SHORT", 13},
    {"FILE_HASH_MISMATCH", 14},
    {"FILE_SAME_AS_SOURCE", 15},
    {"NETWORK_FAILED", 20},
    {"NETWORK_TIMEOUT", 21},
    {"NETWORK_DISCONNECTED", 22},
    {"NETWORK_SERVER_DOWN", 23},
    {"NETWORK_INVALID_REQUEST", 24},
    {"SERVER_FAILED", 30},
    {"SERVER_NO_RANGE", 31},
    {"SERVER_PRECONDITION", 32},
    {"SERVER_BAD_CONTENT", 33},
    {"SERVER_UNAUTHORIZED", 34},
    {"SERVER_CERT_PROBLEM", 35},
    {"SERVER_FORBIDDEN", 36},
    {"SERVER_UNREACHABLE", 37},
    {"SERVER_CONTENT_LENGTH_MISMATCH", 38},
    {"SERVER_CROSS_ORIGIN_REDIRECT", 39},
    {"USER_CANCELED", 40},
    {"USER_SHUTDOWN", 41},
    {"CRASH", 50},
};

// Make sure no one has changed a DownloadInterruptReason we've previously
// persisted.
TEST_F(ContentHistoryBackendDBTest,
       ConfirmDownloadInterruptReasonBackwardsCompatible) {
  // Are there any cases in which a historical number has been repurposed
  // for an error other than it's original?
  for (size_t i = 0; i < std::size(current_reasons); i++) {
    const InterruptReasonAssociation& cur_reason(current_reasons[i]);
    bool found = false;

    for (size_t j = 0; j < std::size(historical_reasons); ++j) {
      const InterruptReasonAssociation& hist_reason(historical_reasons[j]);

      if (hist_reason.value == cur_reason.value) {
        EXPECT_EQ(cur_reason.name, hist_reason.name)
            << "Same integer value used for old error \""
            << hist_reason.name
            << "\" as for new error \""
            << cur_reason.name
            << "\"." << std::endl
            << "**This will cause database conflicts with persisted values**"
            << std::endl
            << "Please assign a new, non-conflicting value for the new error.";
      }

      if (hist_reason.name == cur_reason.name) {
        EXPECT_EQ(cur_reason.value, hist_reason.value)
            << "Same name (\"" << hist_reason.name
            << "\") maps to a different value historically ("
            << hist_reason.value << ") and currently ("
            << cur_reason.value << ")" << std::endl
            << "This may cause database conflicts with persisted values"
            << std::endl
            << "If this error is the same as the old one, you should"
            << std::endl
            << "use the old value, and if it is different, you should"
            << std::endl
            << "use a new name.";

        found = true;
      }
    }

    EXPECT_TRUE(found)
        << "Error \"" << cur_reason.name << "\" not found in historical list."
        << std::endl
        << "Please add it.";
  }
}
}  // namespace
}  // namespace history
