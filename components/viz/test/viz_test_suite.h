// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_VIZ_TEST_SUITE_H_
#define COMPONENTS_VIZ_TEST_VIZ_TEST_SUITE_H_

#include <memory>

#include "base/macros.h"
#include "base/test/task_environment.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite.h"

namespace viz {

class VizTestSuite : public base::TestSuite {
 public:
  VizTestSuite(int argc, char** argv);
  ~VizTestSuite() override;

 protected:
  // Overridden from base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

 private:
  std::unique_ptr<base::test::TaskEnvironment> task_environment_;
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;

  DISALLOW_COPY_AND_ASSIGN(VizTestSuite);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_VIZ_TEST_SUITE_H_
