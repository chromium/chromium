// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_PERFTEST_PERF_TEST_H_
#define COMPONENTS_CRONET_NATIVE_PERFTEST_PERF_TEST_H_

// Run Cronet native performance test. |json_args| is the string containing
// the JSON formatted arguments from components/cronet/native/perftest/run.py.
void PerfTest(const char* json_args);

#endif  // COMPONENTS_CRONET_NATIVE_PERFTEST_PERF_TEST_H_
