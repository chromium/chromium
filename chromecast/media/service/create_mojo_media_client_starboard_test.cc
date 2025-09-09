// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromecast/media/api/test/mock_cma_backend_factory.h"
#include "chromecast/media/service/create_mojo_media_client.h"
#include "chromecast/media/service/video_geometry_setter_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace media {
namespace {

using ::testing::NotNull;

TEST(CreateMojoMediaClientStarboardTest, CreatesStarboardClientWhenFlagIsSet) {
  // This must be destructed last.
  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList features;
  features.InitFromCommandLine(/*enabled_features=*/"enable_starboard_renderer",
                               /*disabled_features=*/"");

  MockCmaBackendFactory cma_factory;
  CreateCdmFactoryCB factory_cb = base::BindRepeating(
      +[](::media::mojom::FrameInterfaceFactory*)
          -> std::unique_ptr<::media::CdmFactory> { return nullptr; });
  VideoGeometrySetterService geometry_setter_service;
  EnableBufferingCB enable_buffering_cb =
      base::BindRepeating(+[]() { return true; });

  EXPECT_THAT(CreateMojoMediaClientForCast(&cma_factory, std::move(factory_cb),
                                           /*video_mode_switcher=*/nullptr,
                                           /*video_resolution_policy=*/nullptr,
                                           &geometry_setter_service,
                                           std::move(enable_buffering_cb)),
              NotNull());
}

TEST(CreateMojoMediaClientStarboardTest, CreatesCmaClientWhenFlagIsNotSet) {
  // This must be destructed last.
  base::test::TaskEnvironment task_environment;
  base::test::ScopedFeatureList features;
  features.InitFromCommandLine(
      /*enabled_features=*/"",
      /*disabled_features=*/"enable_starboard_renderer");

  MockCmaBackendFactory cma_factory;
  CreateCdmFactoryCB factory_cb = base::BindRepeating(
      +[](::media::mojom::FrameInterfaceFactory*)
          -> std::unique_ptr<::media::CdmFactory> { return nullptr; });
  VideoGeometrySetterService geometry_setter_service;
  EnableBufferingCB enable_buffering_cb =
      base::BindRepeating(+[]() { return true; });

  EXPECT_THAT(CreateMojoMediaClientForCast(&cma_factory, std::move(factory_cb),
                                           /*video_mode_switcher=*/nullptr,
                                           /*video_resolution_policy=*/nullptr,
                                           &geometry_setter_service,
                                           std::move(enable_buffering_cb)),
              NotNull());
}

}  // namespace
}  // namespace media
}  // namespace chromecast
