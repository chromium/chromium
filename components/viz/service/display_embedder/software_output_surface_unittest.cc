// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_surface.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "cc/test/fake_output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/vsync_provider.h"

namespace viz {
namespace {

class FakeVSyncProvider : public gfx::VSyncProvider {
 public:
  FakeVSyncProvider() = default;

  FakeVSyncProvider(const FakeVSyncProvider&) = delete;
  FakeVSyncProvider& operator=(const FakeVSyncProvider&) = delete;

  ~FakeVSyncProvider() override = default;

  int call_count() const { return call_count_; }

  // gfx::VSyncProvider implementation.
  void GetVSyncParameters(UpdateVSyncCallback callback) override {
    std::move(callback).Run(base::TimeTicks(), base::TimeDelta());
    call_count_++;
  }

  bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                     base::TimeDelta* interval) override {
    return false;
  }

  bool SupportGetVSyncParametersIfAvailable() const override { return false; }
  bool IsHWClock() const override { return false; }

 private:
  int call_count_ = 0;
};

class VSyncSoftwareOutputDevice : public SoftwareOutputDevice {
 public:
  VSyncSoftwareOutputDevice() = default;

  VSyncSoftwareOutputDevice(const VSyncSoftwareOutputDevice&) = delete;
  VSyncSoftwareOutputDevice& operator=(const VSyncSoftwareOutputDevice&) =
      delete;

  ~VSyncSoftwareOutputDevice() override = default;

  // SoftwareOutputDevice implementation.
  gfx::VSyncProvider* GetVSyncProvider() override { return &vsync_provider_; }

 private:
  FakeVSyncProvider vsync_provider_;
};

}  // namespace

TEST(SoftwareOutputSurfaceTest, NoVSyncProvider) {
  cc::FakeOutputSurfaceClient output_surface_client;
  auto output_surface = std::make_unique<SoftwareOutputSurface>(
      std::make_unique<SoftwareOutputDevice>());
  output_surface->BindToClient(&output_surface_client);

  // Verify the callback is never called.
  output_surface->SetUpdateVSyncParametersCallback(base::BindRepeating(
      [](base::TimeTicks timebase, base::TimeDelta interval) {
        EXPECT_TRUE(false);
      }));

  output_surface->SwapBuffers(OutputSurfaceFrame());
  EXPECT_EQ(nullptr, output_surface->software_device()->GetVSyncProvider());
}

TEST(SoftwareOutputSurfaceTest, VSyncProviderUpdates) {
  cc::FakeOutputSurfaceClient output_surface_client;
  auto output_surface = std::make_unique<SoftwareOutputSurface>(
      std::make_unique<VSyncSoftwareOutputDevice>());
  output_surface->BindToClient(&output_surface_client);

  int update_vsync_parameters_call_count = 0;
  output_surface->SetUpdateVSyncParametersCallback(base::BindLambdaForTesting(
      [&update_vsync_parameters_call_count](base::TimeTicks timebase,
                                            base::TimeDelta interval) {
        ++update_vsync_parameters_call_count;
      }));

  FakeVSyncProvider* vsync_provider = static_cast<FakeVSyncProvider*>(
      output_surface->software_device()->GetVSyncProvider());
  EXPECT_EQ(0, vsync_provider->call_count());

  // Verify that we get vsync parameters from the VSyncProvider and provide them
  // to the callback.
  output_surface->SwapBuffers(OutputSurfaceFrame());
  EXPECT_EQ(1, vsync_provider->call_count());
  EXPECT_EQ(1, update_vsync_parameters_call_count);
}

}  // namespace viz
