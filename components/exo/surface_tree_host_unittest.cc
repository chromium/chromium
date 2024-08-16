// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/exo/surface_tree_host.h"

#include <memory>
#include <utility>

#include "ash/display/display_configuration_controller.h"
#include "ash/shell.h"
#include "base/test/bind.h"
#include "components/exo/shell_surface.h"
#include "components/exo/sub_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/config/gpu_feature_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/types/display_constants.h"

using ::testing::InSequence;

namespace exo {
namespace {

class SurfaceTreeHostTest : public test::ExoTestBase {
 protected:
  void SetUp() override {
    test::ExoTestBase::SetUp();

    shell_surface_ = test::ShellSurfaceBuilder({16, 16}).BuildShellSurface();
  }

  void TearDown() override {
    shell_surface_.reset();

    test::ExoTestBase::TearDown();
  }

  ash::DisplayConfigurationController* display_config_controller() {
    return ash::Shell::Get()->display_configuration_controller();
  }

  std::unique_ptr<ShellSurface> shell_surface_;
};

class LayoutManagerChecker : public aura::LayoutManager {
 public:
  void OnWindowAddedToLayout(aura::Window* child) override {}
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {}

  MOCK_METHOD(void, OnWindowResized, (), (override));
};

}  // namespace

TEST_F(SurfaceTreeHostTest, UpdatePrimaryDisplayWithSurfaceUpdateFailure) {
  UpdateDisplay("800x600,1000x800@1.2");
  display::Display display1 = GetPrimaryDisplay();
  display::Display display2 = GetSecondaryDisplay();

  std::vector<std::pair<int64_t, int64_t>> leave_enter_ids;
  bool callback_return_value = true;
  shell_surface_->root_surface()->set_leave_enter_callback(
      base::BindLambdaForTesting(
          [&leave_enter_ids, &callback_return_value](int64_t old_display_id,
                                                     int64_t new_display_id) {
            leave_enter_ids.emplace_back(old_display_id, new_display_id);
            return callback_return_value;
          }));

  // Successfully update surface to display 2.
  display_config_controller()->SetPrimaryDisplayId(display2.id(), false);
  ASSERT_EQ(leave_enter_ids.size(), 1u);
  EXPECT_EQ(leave_enter_ids[0], std::make_pair(display1.id(), display2.id()));

  // Fail to update surface to display 1.
  callback_return_value = false;
  display_config_controller()->SetPrimaryDisplayId(display1.id(), false);
  ASSERT_EQ(leave_enter_ids.size(), 2u);
  EXPECT_EQ(leave_enter_ids[1], std::make_pair(display2.id(), display1.id()));

  // Should still send an update for surface to enter display 2.
  callback_return_value = true;
  display_config_controller()->SetPrimaryDisplayId(display2.id(), false);
  ASSERT_EQ(leave_enter_ids.size(), 3u);
  EXPECT_EQ(leave_enter_ids[2],
            std::make_pair(display::kInvalidDisplayId, display2.id()));
}

TEST_F(SurfaceTreeHostTest,
       BuiltinDisplayMirrorModeToExtendModeWithExternalDisplayAsPrimary) {
  UpdateDisplay("800x600,1000x800@1.2");

  // Set first display as internal, so it'll be primary source in mirror mode.
  int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  int64_t external_display_id = GetSecondaryDisplay().id();

  ASSERT_NE(internal_display_id, external_display_id);

  std::vector<std::pair<int64_t, int64_t>> leave_enter_ids;
  shell_surface_->root_surface()->set_leave_enter_callback(
      base::BindLambdaForTesting(
          [&leave_enter_ids](int64_t old_display_id, int64_t new_display_id) {
            leave_enter_ids.emplace_back(old_display_id, new_display_id);
            return true;
          }));

  // Make external display primary.
  display_config_controller()->SetPrimaryDisplayId(external_display_id, false);

  ASSERT_EQ(leave_enter_ids.size(), 1u);
  EXPECT_EQ(leave_enter_ids[0],
            std::make_pair(internal_display_id, external_display_id));

  // Change to mirror mode, which should make internal display primary.
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(leave_enter_ids.size(), 2u);
  EXPECT_EQ(leave_enter_ids[1],
            std::make_pair(external_display_id, internal_display_id));

  // Switch back to extend mode, which should restore external as primary.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(leave_enter_ids.size(), 3u);
  EXPECT_EQ(leave_enter_ids[2],
            std::make_pair(internal_display_id, external_display_id));
}

TEST_F(SurfaceTreeHostTest,
       UpdateHostWindowBoundsOnlySetsNewBoundsIfContentSizeChanged) {
  // Create 50x50 shell surface.
  auto shell_surface = test::ShellSurfaceBuilder({50, 50}).BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  // Create 25x25 sub surface.
  auto child_surface = std::make_unique<Surface>();
  auto child_buffer = test::ExoTestHelper::CreateBuffer(gfx::Size(25, 25));
  auto sub_surface = std::make_unique<SubSurface>(child_surface.get(), surface);
  child_surface->Attach(child_buffer.get());
  child_surface->Commit();

  // Set a mocked LayoutManager for testing purposes.
  shell_surface->host_window()->SetLayoutManager(
      std::make_unique<LayoutManagerChecker>());
  auto* layout_manager_checker = static_cast<LayoutManagerChecker*>(
      shell_surface->host_window()->layout_manager());

  {
    InSequence s;

    // SetBounds (and hence OnWindowResized) is called once when changing
    // content bounds.
    EXPECT_CALL(*layout_manager_checker, OnWindowResized).Times(1);
    sub_surface->SetPosition({50, 50});
    surface->Commit();
    EXPECT_EQ(gfx::Rect(0, 0, 75, 75), shell_surface->host_window()->bounds());

    // SetBounds (and hence OnWindowResized) is not called when
    // UpdateHostWindowBounds() is called but content bounds have not changed
    // in DP.
    EXPECT_CALL(*layout_manager_checker, OnWindowResized).Times(0);
    surface->Commit();
    EXPECT_EQ(gfx::Rect(0, 0, 75, 75), shell_surface->host_window()->bounds());

    // SetBounds (and hence OnWindowResized) is called once when
    // destroying the root surface.
    EXPECT_CALL(*layout_manager_checker, OnWindowResized).Times(1);
    test::ShellSurfaceBuilder::DestroyRootSurface(shell_surface.get());
    EXPECT_EQ(gfx::Rect(0, 0, 0, 0), shell_surface->host_window()->bounds());
  }
}

TEST_F(SurfaceTreeHostTest,
       UpdateHostWindowBoundsAllocatesLocalSurfaceIdWhenPixelSizeOnlyChanges) {
  // Set device scale factor to 300%.
  UpdateDisplay("800x600*3");

  // Create 50x50 shell surface which submits in pixel coordinates.
  auto shell_surface = test::ShellSurfaceBuilder({50, 50})
                           .SetClientSubmitsInPixelCoordinates(true)
                           .BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  // Create 1x1 sub surface.
  auto child_surface = std::make_unique<Surface>();
  auto child_buffer = test::ExoTestHelper::CreateBuffer(gfx::Size(1, 1));
  auto sub_surface = std::make_unique<SubSurface>(child_surface.get(), surface);
  child_surface->Attach(child_buffer.get());
  child_surface->Commit();

  // Set a mocked LayoutManager for testing purposes.
  shell_surface->host_window()->SetLayoutManager(
      std::make_unique<LayoutManagerChecker>());
  auto* layout_manager_checker = static_cast<LayoutManagerChecker*>(
      shell_surface->host_window()->layout_manager());

  // The 50x50 content bound is scaled to 17x17.
  EXPECT_EQ(gfx::Rect(0, 0, 17, 17), shell_surface->host_window()->bounds());

  {
    InSequence s;

    // Set a 51x51 content bound (also scaled to 17).
    // AllocateLocalSurfaceId is called here because DP size has not changed,
    // but pixel size has, so we need a new surface id.
    // SetBounds (and hence OnWindowResized) is not called.
    EXPECT_CALL(*layout_manager_checker, OnWindowResized).Times(0);
    auto local_surface_id = shell_surface->host_window()->GetLocalSurfaceId();
    sub_surface->SetPosition({50, 50});
    surface->Commit();
    EXPECT_EQ(gfx::Rect(0, 0, 17, 17), shell_surface->host_window()->bounds());
    EXPECT_NE(shell_surface->host_window()->GetLocalSurfaceId(),
              local_surface_id);

    // If we try again with the same pixel size, no surface id will be
    // allocated.
    // SetBounds (and hence OnWindowResized) is not called.
    EXPECT_CALL(*layout_manager_checker, OnWindowResized).Times(0);
    local_surface_id = shell_surface->host_window()->GetLocalSurfaceId();
    surface->Commit();
    EXPECT_EQ(gfx::Rect(0, 0, 17, 17), shell_surface->host_window()->bounds());
    EXPECT_EQ(shell_surface->host_window()->GetLocalSurfaceId(),
              local_surface_id);

    // SetBounds (and hence OnWindowResized) is called once when
    // destroying the root surface.
    EXPECT_CALL(*layout_manager_checker, OnWindowResized).Times(1);
    test::ShellSurfaceBuilder::DestroyRootSurface(shell_surface.get());
    EXPECT_EQ(gfx::Rect(0, 0, 0, 0), shell_surface->host_window()->bounds());
  }
}

TEST_F(SurfaceTreeHostTest,
       UpdateScaleFactorUpdatesHostWindowBoundsEvenWhenPixelSizeIsSame) {
  // Create 50x50 shell surface.
  auto shell_surface = test::ShellSurfaceBuilder({50, 50})
                           .SetClientSubmitsInPixelCoordinates(true)
                           .BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  // Create 25x25 sub surface.
  auto child_surface = std::make_unique<Surface>();
  auto child_buffer = test::ExoTestHelper::CreateBuffer(gfx::Size(25, 25));
  auto sub_surface = std::make_unique<SubSurface>(child_surface.get(), surface);
  child_surface->Attach(child_buffer.get());
  child_surface->Commit();

  // Set a mocked LayoutManager for testing purposes.
  shell_surface->host_window()->SetLayoutManager(
      std::make_unique<LayoutManagerChecker>());
  auto* layout_manager_checker = static_cast<LayoutManagerChecker*>(
      shell_surface->host_window()->layout_manager());

  {
    InSequence s;

    // SetBounds (and hence OnWindowResized) is called once when changing
    // content bounds.
    EXPECT_CALL(*layout_manager_checker, OnWindowResized).Times(1);
    sub_surface->SetPosition({50, 50});
    surface->Commit();
    EXPECT_EQ(gfx::Rect(0, 0, 75, 75), shell_surface->host_window()->bounds());

    // Changing scale factor can affect host window size as it's in DP
    // coordinate.
    EXPECT_CALL(*layout_manager_checker, OnWindowResized).Times(1);
    shell_surface->SetScaleFactor(2.f);
    shell_surface->host_window()->AllocateLocalSurfaceId();
    surface->Commit();
    EXPECT_EQ(gfx::Rect(0, 0, 38, 38), shell_surface->host_window()->bounds());

    // SetBounds (and hence OnWindowResized) is called once when
    // destroying the root surface.
    EXPECT_CALL(*layout_manager_checker, OnWindowResized).Times(1);
    test::ShellSurfaceBuilder::DestroyRootSurface(shell_surface.get());
    EXPECT_EQ(gfx::Rect(0, 0, 0, 0), shell_surface->host_window()->bounds());
  }
}

namespace {

//
class InterceptingTestRasterInterface : public viz::TestRasterInterface {
 public:
  InterceptingTestRasterInterface() = default;
  ~InterceptingTestRasterInterface() override = default;

  // Returns verified and unverified sync tokens the raster interface received
  // via VerifySyncTokensCHROMIUM.
  std::pair<int, int> GetAndResetSyncTokensCount() {
    auto verified_tokens = verified_sync_tokens_;
    auto unverified_tokens = unverified_sync_tokens_;
    ResetSyncTokensCount();
    return {verified_tokens, unverified_tokens};
  }

  void ResetSyncTokensCount() {
    verified_sync_tokens_ = 0;
    unverified_sync_tokens_ = 0;
  }

  // gpu::raster::RasterInterface overrides:
  void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override {
    ResetSyncTokensCount();
    for (GLsizei i = 0; i < count; ++i) {
      gpu::SyncToken sync_token_data;
      memcpy(sync_token_data.GetData(), sync_tokens[i],
             sizeof(sync_token_data));
      if (sync_token_data.verified_flush()) {
        verified_sync_tokens_++;
      } else {
        unverified_sync_tokens_++;
      }
    }
    viz::TestRasterInterface::VerifySyncTokensCHROMIUM(sync_tokens, count);
  }

 private:
  int verified_sync_tokens_ = 0;
  int unverified_sync_tokens_ = 0;
};

class FakeRasterContextProvider
    : public base::RefCountedThreadSafe<FakeRasterContextProvider>,
      public viz::RasterContextProvider {
 public:
  FakeRasterContextProvider() = default;

  FakeRasterContextProvider(FakeRasterContextProvider&) = delete;
  FakeRasterContextProvider& operator=(FakeRasterContextProvider&) = delete;

  void SetOnDestroyedClosure(base::OnceClosure on_destroyed) {
    on_destroyed_ = std::move(on_destroyed);
  }

  // viz::RasterContextProvider implementation;
  void AddRef() const override {
    base::RefCountedThreadSafe<FakeRasterContextProvider>::AddRef();
  }
  void Release() const override {
    base::RefCountedThreadSafe<FakeRasterContextProvider>::Release();
  }
  gpu::ContextResult BindToCurrentSequence() override {
    ADD_FAILURE();
    return gpu::ContextResult::kFatalFailure;
  }
  void AddObserver(viz::ContextLostObserver* obs) override { ADD_FAILURE(); }
  void RemoveObserver(viz::ContextLostObserver* obs) override { ADD_FAILURE(); }
  base::Lock* GetLock() override {
    ADD_FAILURE();
    return nullptr;
  }
  viz::ContextCacheController* CacheController() override {
    ADD_FAILURE();
    return nullptr;
  }
  gpu::ContextSupport* ContextSupport() override { return nullptr; }
  class GrDirectContext* GrContext() override {
    ADD_FAILURE();
    return nullptr;
  }
  gpu::SharedImageInterface* SharedImageInterface() override {
    ADD_FAILURE();
    return nullptr;
  }
  const gpu::Capabilities& ContextCapabilities() const override {
    ADD_FAILURE();
    static gpu::Capabilities dummy_caps;
    return dummy_caps;
  }
  const gpu::GpuFeatureInfo& GetGpuFeatureInfo() const override {
    ADD_FAILURE();
    static gpu::GpuFeatureInfo dummy_feature_info;
    return dummy_feature_info;
  }
  gpu::raster::RasterInterface* RasterInterface() override {
    return GetInterceptingTestRasterInterface();
  }
  unsigned int GetGrGLTextureFormat(
      viz::SharedImageFormat format) const override {
    ADD_FAILURE();
    return 0;
  }

  InterceptingTestRasterInterface* GetInterceptingTestRasterInterface() {
    return &intercepting_test_raster_interface_;
  }

 private:
  friend class base::RefCountedThreadSafe<FakeRasterContextProvider>;

  ~FakeRasterContextProvider() override {
    if (on_destroyed_) {
      std::move(on_destroyed_).Run();
    }
  }

  base::OnceClosure on_destroyed_;

  InterceptingTestRasterInterface intercepting_test_raster_interface_;
};

}  // namespace

// The SurfaceTreeHost can set sync tokens as verified in advance to have less
// load on the IPC if they were verified in the previous frame.
TEST_F(SurfaceTreeHostTest, DoesntVerifyVerifiedSyncTokens) {
  auto shell_surface = test::ShellSurfaceBuilder({50, 50})
                           .SetClientSubmitsInPixelCoordinates(true)
                           .BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  scoped_refptr<FakeRasterContextProvider> ctx_prodiver =
      base::MakeRefCounted<FakeRasterContextProvider>();
  auto old_provider =
      shell_surface->SetRasterContextProviderForTesting(ctx_prodiver);

  auto* raster_interface = ctx_prodiver->GetInterceptingTestRasterInterface();
  raster_interface->ResetSyncTokensCount();

  // Create a buffer and attach it to the surface.
  auto buffer = test::ExoTestHelper::CreateBuffer(gfx::Size(50, 50));
  surface->Attach(buffer.get());

  surface->Commit();

  // Its a new buffer and a newly generated sync token that shouldn't be known
  // by the surface_tree_host. Thus, it must be unverified.
  std::pair<int, int> sync_tokens_count =
      raster_interface->GetAndResetSyncTokensCount();
  EXPECT_EQ(0, sync_tokens_count.first);
  EXPECT_EQ(1, sync_tokens_count.second);

  // Commit the same buffer the second time.
  surface->Commit();
  // Same buffer, nothing has been changed. The sync token is known by the
  // surface_tree_host. Thus, it must be a verified token.
  sync_tokens_count = raster_interface->GetAndResetSyncTokensCount();
  EXPECT_EQ(1, sync_tokens_count.first);
  EXPECT_EQ(0, sync_tokens_count.second);

  shell_surface->SetRasterContextProviderForTesting(old_provider);
}

}  // namespace exo
