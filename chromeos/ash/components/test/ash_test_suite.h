// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TEST_ASH_TEST_SUITE_H_
#define CHROMEOS_ASH_COMPONENTS_TEST_ASH_TEST_SUITE_H_

#include <memory>

#include "base/test/test_discardable_memory_allocator.h"
#include "base/test/test_suite.h"

namespace aura {
class Env;
}

namespace ash {

class AshTestSuite : public base::TestSuite {
 public:
  AshTestSuite(int argc, char** argv);
  AshTestSuite(const AshTestSuite&) = delete;
  AshTestSuite& operator=(const AshTestSuite&) = delete;
  ~AshTestSuite() override;

  static void LoadTestResources();

 protected:
  // base::TestSuite:
  void Initialize() override;
  void Shutdown() override;

 private:
  std::unique_ptr<aura::Env> env_;
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TEST_ASH_TEST_SUITE_H_
