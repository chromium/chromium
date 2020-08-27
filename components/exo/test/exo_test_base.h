// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_EXO_TEST_BASE_H_
#define COMPONENTS_EXO_TEST_EXO_TEST_BASE_H_

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "components/exo/test/exo_test_helper.h"

namespace viz {
class SurfaceManager;
}

namespace exo {
class WMHelper;
class Buffer;

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

  ~ExoTestBase() override;

  // TODO(oshima): Convert unit tests to use this.
  class ShellSurfaceHolder {
   public:
    ShellSurfaceHolder(std::unique_ptr<Buffer> buffer,
                       std::unique_ptr<Surface> surface,
                       std::unique_ptr<ShellSurface> shell_surface);
    ~ShellSurfaceHolder();
    ShellSurfaceHolder(const ShellSurfaceHolder&) = delete;
    ShellSurfaceHolder& operator=(const ShellSurfaceHolder&) = delete;

    ShellSurface* shell_surface() { return shell_surface_.get(); }

   private:
    std::unique_ptr<Buffer> buffer_;
    std::unique_ptr<Surface> surface_;
    std::unique_ptr<ShellSurface> shell_surface_;
  };

  // ash::AshTestBase:
  void SetUp() override;
  void TearDown() override;

  viz::SurfaceManager* GetSurfaceManager();

  std::unique_ptr<ShellSurfaceHolder> CreateShellSurfaceHolder(
      const gfx::Size& buffer_size,
      ShellSurface* parent);

  ExoTestHelper* exo_test_helper() { return &exo_test_helper_; }

 private:
  ExoTestHelper exo_test_helper_;
  std::unique_ptr<WMHelper> wm_helper_;

  DISALLOW_COPY_AND_ASSIGN(ExoTestBase);
};

}  // namespace test
}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_EXO_TEST_BASE_H_
