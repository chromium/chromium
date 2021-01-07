// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/exo_test_base.h"

#include "ash/shell.h"
#include "components/exo/buffer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "components/exo/wm_helper_chromeos.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "ui/aura/client/aura_constants.h"
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
      const std::string& app_id,
      const std::string& startup_id,
      bool for_creation,
      ui::PropertyHandler& out_properties_container) override {
    LOG(ERROR) << "AppId=" << app_id;
    if (app_id == "arc")
      out_properties_container.SetProperty(aura::client::kAppType,
                                           (int)ash::AppType::ARC_APP);
  }
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExoTestBase, public:

ExoTestBase::ShellSurfaceHolder::ShellSurfaceHolder(
    std::unique_ptr<Buffer> buffer,
    std::unique_ptr<Surface> surface,
    std::unique_ptr<ShellSurface> shell_surface)
    : buffer_(std::move(buffer)),
      surface_(std::move(surface)),
      shell_surface_(std::move(shell_surface)) {}

ExoTestBase::ShellSurfaceHolder::~ShellSurfaceHolder() = default;

ExoTestBase::ExoTestBase() = default;

ExoTestBase::~ExoTestBase() = default;

void ExoTestBase::SetUp() {
  AshTestBase::SetUp();
  wm_helper_ = std::make_unique<WMHelperChromeOS>();
  wm_helper_->RegisterAppPropertyResolver(
      base::WrapUnique(new TestPropertyResolver()));
}

void ExoTestBase::TearDown() {
  wm_helper_.reset();
  AshTestBase::TearDown();
}

viz::SurfaceManager* ExoTestBase::GetSurfaceManager() {
  return static_cast<ui::InProcessContextFactory*>(
             aura::Env::GetInstance()->context_factory())
      ->GetFrameSinkManager()
      ->surface_manager();
}

std::unique_ptr<ExoTestBase::ShellSurfaceHolder>
ExoTestBase::CreateShellSurfaceHolder(const gfx::Size& buffer_size,
                                      ShellSurface* parent) {
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  if (parent)
    shell_surface->SetParent(parent);
  surface->Attach(buffer.get());
  surface->Commit();
  return std::make_unique<ShellSurfaceHolder>(
      std::move(buffer), std::move(surface), std::move(shell_surface));
}

}  // namespace test
}  // namespace exo
