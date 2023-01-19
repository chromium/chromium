// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_EXO_TEST_BASE_H_
#define COMPONENTS_EXO_TEST_EXO_TEST_BASE_H_

#include <memory>

#include "ash/test/ash_test_base.h"
#include "components/exo/test/exo_test_helper.h"

namespace ash {
class TestShellDelegate;
}

namespace viz {
class SurfaceManager;
}

namespace exo {
class WMHelper;
class ShellSurfaceBase;

namespace test {
class ExoTestHelper;

class ExoTestBase : public ash::AshTestBase {
 public:
  ExoTestBase();

  // Constructs an ExoTestBase with |traits| being forwarded to its
  // TaskEnvironment. See the corresponding |AshTestBase| constructor.
  template <typename... TaskEnvironmentTraits>
  NOINLINE explicit ExoTestBase(TaskEnvironmentTraits&&... traits)
      : AshTestBase(std::forward<TaskEnvironmentTraits>(traits)...) {}

  ExoTestBase(const ExoTestBase&) = delete;
  ExoTestBase& operator=(const ExoTestBase&) = delete;

  ~ExoTestBase() override;

  // ash::AshTestBase:
  void SetUp() override;
  void TearDown() override;

  void SetUp(std::unique_ptr<ash::TestShellDelegate> shell_delegate);

  viz::SurfaceManager* GetSurfaceManager();

  gfx::Point GetOriginOfShellSurface(const ShellSurfaceBase* shell_surface);

  ExoTestHelper* exo_test_helper() { return &exo_test_helper_; }
  WMHelper* wm_helper() { return wm_helper_.get(); }

 private:
  ExoTestHelper exo_test_helper_;
  std::unique_ptr<WMHelper> wm_helper_;
};

}  // namespace test
}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_EXO_TEST_BASE_H_
