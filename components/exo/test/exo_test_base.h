// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_EXO_TEST_BASE_H_
#define COMPONENTS_EXO_TEST_EXO_TEST_BASE_H_

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace viz {
class SurfaceManager;
}

namespace exo {
class WMHelper;

namespace test {
class ExoTestHelper;

class ExoTestBase : public ash::AshTestBase {
 public:
  ExoTestBase();
  ~ExoTestBase() override;

  // Overridden from testing::Test:
  void SetUp() override;
  void TearDown() override;

  viz::SurfaceManager* GetSurfaceManager();

  ExoTestHelper* exo_test_helper() { return exo_test_helper_.get(); }

 private:
  std::unique_ptr<ExoTestHelper> exo_test_helper_;
  std::unique_ptr<WMHelper> wm_helper_;
  ui::ScopedAnimationDurationScaleMode scale_mode_;

  DISALLOW_COPY_AND_ASSIGN(ExoTestBase);
};

}  // namespace test
}  // namespace exo

#endif  // COMPONENTS_EXO_TEST_EXO_TEST_BASE_H_
