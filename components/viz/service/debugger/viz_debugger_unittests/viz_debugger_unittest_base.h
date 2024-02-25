// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DEBUGGER_VIZ_DEBUGGER_UNITTESTS_VIZ_DEBUGGER_UNITTEST_BASE_H_
#define COMPONENTS_VIZ_SERVICE_DEBUGGER_VIZ_DEBUGGER_UNITTESTS_VIZ_DEBUGGER_UNITTEST_BASE_H_

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "components/viz/service/debugger/viz_debugger_unittests/viz_debugger_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_VIZ_DEBUGGER)

namespace viz {

struct TestFilter {
  std::string anno;
  std::string func;
  std::string file;
  bool active = true;
  bool enabled = true;

  TestFilter();

  ~TestFilter();

  explicit TestFilter(const std::string& anno_);

  TestFilter(const std::string& anno_, const std::string& func_);

  TestFilter(const std::string& anno_,
             const std::string& func_,
             const std::string& file_);

  TestFilter(const std::string& anno_,
             const std::string& func_,
             const std::string& file_,
             const bool& active_,
             const bool& enabled_);

  TestFilter(const TestFilter& other);
};

struct StaticSource {
  std::string file;
  std::string func;
  std::string anno;
  int line;
  int index;

  StaticSource();
  ~StaticSource();
  StaticSource(const StaticSource& other);
};

// The VisualDebuggerTestBase class is the base unit test class used for
// multiple VizDebugger unit tests (VisualDebuggerUnitTest and
// VizDebuggerMultithreadTest). This class is inherited by the different
// unit tests for use.
class VisualDebuggerTestBase : public testing::Test {
 protected:
  VizDebuggerInternal* GetInternal();

  void SetUp() override;
  void TearDown() override;

  void SetFilter(std::vector<TestFilter> filters);

 public:
  VisualDebuggerTestBase();
  ~VisualDebuggerTestBase() override;

  // Gets frame data from VizDebugger. Takes in boolean that will
  // either clear the cached results of GetFrameData or not.
  void GetFrameData(bool clear_cache);

  uint64_t frame_counter_ = 0;

  // Cached result of call to 'GetFrameData' to simplify code.
  uint64_t counter_;
  int window_x_ = 256;
  int window_y_ = 256;
  std::vector<StaticSource> sources_cache_;
  std::vector<VizDebuggerInternal::DrawCall> draw_calls_cache_;
  std::vector<VizDebuggerInternal::LogCall> log_calls_cache_;
  std::vector<VizDebuggerInternal::Buffer> buffers_;
};
}  // namespace viz

#endif  // BUILDFLAG(USE_VIZ_DEBUGGER)
#endif  // COMPONENTS_VIZ_SERVICE_DEBUGGER_VIZ_DEBUGGER_UNITTESTS_VIZ_DEBUGGER_UNITTEST_BASE_H_
