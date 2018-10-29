// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/native/perftest/perf_test.h"

#include "base/logging.h"

// When invoked, passes first and only argument to native performance test.
int main(int argc, char* argv[]) {
  CHECK_EQ(argc, 2) << "Must include experimental options in JSON as only arg.";
  PerfTest(argv[1]);
  return 0;
}