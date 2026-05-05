// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/capture_mode/capture_mode_util.h"

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"

namespace capture_mode {

namespace {

class MockContextFactory : public ui::ContextFactory {
 public:
  MockContextFactory() = default;
  ~MockContextFactory() override = default;

  void CreateLayerTreeFrameSink(
      base::WeakPtr<ui::Compositor> compositor) override {}
  scoped_refptr<viz::RasterContextProvider>
  SharedMainThreadRasterContextProvider() override {
    return provider_;
  }
  void RemoveCompositor(ui::Compositor* compositor) override {}
  cc::TaskGraphRunner* GetTaskGraphRunner() override { return nullptr; }
  viz::FrameSinkId AllocateFrameSinkId() override { return viz::FrameSinkId(); }
  viz::SubtreeCaptureId AllocateSubtreeCaptureId() override {
    return viz::SubtreeCaptureId();
  }
  viz::HostFrameSinkManager* GetHostFrameSinkManager() override {
    return nullptr;
  }

  void set_provider(scoped_refptr<viz::RasterContextProvider> provider) {
    provider_ = std::move(provider);
  }

 private:
  scoped_refptr<viz::RasterContextProvider> provider_;
};

TEST(CaptureModeUtilTest, IsGpuRasterizationSupported_NullProvider) {
  MockContextFactory factory;
  factory.set_provider(nullptr);

  // This should not crash and should return false.
  EXPECT_FALSE(IsGpuRasterizationSupported(&factory));
}

TEST(CaptureModeUtilTest,
     IsGpuRasterizationSupported_GpuRasterizationDisabled) {
  auto provider = viz::TestContextProvider::CreateRaster();
  provider->GetWritableGpuFeatureInfo()
      .status_values[gpu::GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] =
      gpu::kGpuFeatureStatusDisabled;
  provider->BindToCurrentSequence();

  MockContextFactory factory;
  factory.set_provider(std::move(provider));

  EXPECT_FALSE(IsGpuRasterizationSupported(&factory));
}

TEST(CaptureModeUtilTest, IsGpuRasterizationSupported_GpuRasterizationEnabled) {
  auto provider = viz::TestContextProvider::CreateRaster();
  provider->GetWritableGpuFeatureInfo()
      .status_values[gpu::GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] =
      gpu::kGpuFeatureStatusEnabled;
  provider->BindToCurrentSequence();

  MockContextFactory factory;
  factory.set_provider(std::move(provider));

  EXPECT_TRUE(IsGpuRasterizationSupported(&factory));
}

TEST(CaptureModeUtilTest, IsGpuRasterizationSupported_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Since features::kUiGpuRasterization is hidden, we use the string directly
  // here.
  scoped_feature_list.InitFromCommandLine("", "UiGpuRasterization");

  auto provider = viz::TestContextProvider::CreateRaster();
  provider->GetWritableGpuFeatureInfo()
      .status_values[gpu::GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] =
      gpu::kGpuFeatureStatusEnabled;
  provider->BindToCurrentSequence();

  MockContextFactory factory;
  factory.set_provider(std::move(provider));

  // Even if the GPU status is enabled, the feature is disabled.
  EXPECT_FALSE(IsGpuRasterizationSupported(&factory));
}

}  // namespace

}  // namespace capture_mode
