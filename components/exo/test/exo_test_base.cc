// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_base.h"

#include "ash/shell.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/wm_helper.h"
#include "components/exo/wm_helper_chromeos.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "ui/aura/env.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/wm/core/wm_core_switches.h"

namespace exo {
namespace test {

////////////////////////////////////////////////////////////////////////////////
// ExoTestBase, public:

ExoTestBase::ExoTestBase()
    : exo_test_helper_(new ExoTestHelper),
      scale_mode_(ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {}

ExoTestBase::~ExoTestBase() {}

void ExoTestBase::SetUp() {
  AshTestBase::SetUp();
  wm_helper_ = std::make_unique<WMHelperChromeOS>();
  WMHelper::SetInstance(wm_helper_.get());
}

void ExoTestBase::TearDown() {
  WMHelper::SetInstance(nullptr);
  wm_helper_.reset();
  AshTestBase::TearDown();
}

viz::SurfaceManager* ExoTestBase::GetSurfaceManager() {
  return static_cast<ui::InProcessContextFactory*>(
             aura::Env::GetInstance()->context_factory_private())
      ->GetFrameSinkManager()
      ->surface_manager();
}

}  // namespace test
}  // namespace exo
