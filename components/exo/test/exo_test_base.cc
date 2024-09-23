// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_base.h"

#include "ash/shell.h"
#include "ash/test_shell_delegate.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/buffer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "ui/aura/env.h"
#include "ui/base/ime/init/input_method_factory.h"
#include "ui/compositor/test/in_process_context_factory.h"
#include "ui/wm/core/wm_core_switches.h"

namespace exo {
namespace test {
namespace {

class TestPropertyResolver : public WMHelper::AppPropertyResolver {
 public:
  TestPropertyResolver() = default;
  TestPropertyResolver(const TestPropertyResolver& other) = delete;
  TestPropertyResolver& operator=(const TestPropertyResolver& other) = delete;
  ~TestPropertyResolver() override = default;

  // AppPropertyResolver:
  void PopulateProperties(
      const Params& params,
      ui::PropertyHandler& out_properties_container) override {
    if (params.app_id == "arc") {
      out_properties_container.SetProperty(chromeos::kAppTypeKey,
                                           chromeos::AppType::ARC_APP);
    }
  }
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExoTestBase, public:

ExoTestBase::ExoTestBase() = default;

ExoTestBase::~ExoTestBase() = default;

void ExoTestBase::SetUp() {
  SetUp(nullptr);

  if (task_environment()->UsesMockTime()) {
    // Reduce the refresh rate to save cost for fast forwarding when mock time
    // is used.
    GetContextFactory()->SetRefreshRateForTests(10.0);
  }
}

void ExoTestBase::TearDown() {
  wm_helper_.reset();
  AshTestBase::TearDown();
}

void ExoTestBase::SetUp(
    std::unique_ptr<ash::TestShellDelegate> shell_delegate) {
  AshTestBase::SetUp(std::move(shell_delegate));
  wm_helper_ = std::make_unique<WMHelper>();
  wm_helper_->RegisterAppPropertyResolver(
      base::WrapUnique(new TestPropertyResolver()));
}

viz::SurfaceManager* ExoTestBase::GetSurfaceManager() {
  return static_cast<ui::InProcessContextFactory*>(
             aura::Env::GetInstance()->context_factory())
      ->GetFrameSinkManager()
      ->surface_manager();
}

gfx::Point ExoTestBase::GetOriginOfShellSurface(
    const ShellSurfaceBase* shell_surface) {
  return shell_surface->GetWidget()->GetWindowBoundsInScreen().origin();
}

}  // namespace test
}  // namespace exo
