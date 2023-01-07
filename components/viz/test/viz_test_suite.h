// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_VIZ_TEST_SUITE_H_
#define COMPONENTS_VIZ_TEST_VIZ_TEST_SUITE_H_

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite.h"

namespace ui {
class PlatformEventSource;
}  // namespace ui

namespace viz {

class VizTestSuite : public base::TestSuite {
 public:
  VizTestSuite(int argc, char** argv);

  VizTestSuite(const VizTestSuite&) = delete;
  VizTestSuite& operator=(const VizTestSuite&) = delete;

  ~VizTestSuite() override;

  static void RunUntilIdle();

 protected:
  // Overridden from base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

 private:
  static std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  std::unique_ptr<ui::PlatformEventSource> platform_event_source_;

  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_VIZ_TEST_SUITE_H_
