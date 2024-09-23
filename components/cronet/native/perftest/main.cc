// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/cronet/native/perftest/perf_test.h"

#include <ostream>

#include "base/check_op.h"

// When invoked, passes first and only argument to native performance test.
int main(int argc, char* argv[]) {
  CHECK_EQ(argc, 2) << "Must include experimental options in JSON as only arg.";
  PerfTest(argv[1]);
  return 0;
}
