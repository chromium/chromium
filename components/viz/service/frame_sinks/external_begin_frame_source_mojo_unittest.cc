// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_mojo.h"

#include <memory>

#include "base/functional/callback.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

constexpr FrameSinkId kFrameSinkId(1, 1);

BeginFrameArgs CreateBeginFrameArgsWithSourceId(uint64_t source_id) {
  return CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, source_id,
                                        /*sequence_number=*/1);
}

class ExternalBeginFrameSourceMojoTest : public testing::Test {
 public:
  ExternalBeginFrameSourceMojoTest() = default;
  ~ExternalBeginFrameSourceMojoTest() override = default;

  std::unique_ptr<ExternalBeginFrameSourceMojo> CreateSource() {
    mojo::AssociatedRemote<mojom::ExternalBeginFrameController> controller;
    return std::make_unique<ExternalBeginFrameSourceMojo>(
        &frame_sink_manager_,
        controller.BindNewEndpointAndPassDedicatedReceiver(),
        mojo::NullAssociatedRemote(), BeginFrameSource::kNotRestartableId);
  }

  void DidBeginFrame(const BeginFrameArgs& args) {
    frame_sink_manager_.DidBeginFrame(kFrameSinkId, args);
  }

  void DidFinishFrame(const BeginFrameArgs& args) {
    frame_sink_manager_.DidFinishFrame(kFrameSinkId, args);
  }

 private:
  FrameSinkManagerImpl frame_sink_manager_{
      FrameSinkManagerImpl::InitParams(/*output_surface_provider=*/nullptr)};
};

TEST_F(ExternalBeginFrameSourceMojoTest,
       UnactivatedSourceIgnoresStartingSourceIdBeginFrame) {
  auto source = CreateSource();
  const BeginFrameArgs args =
      CreateBeginFrameArgsWithSourceId(BeginFrameArgs::kStartingSourceId);

  DidBeginFrame(args);

  EXPECT_TRUE(source->pending_frame_sinks_for_testing().empty());
}

TEST_F(ExternalBeginFrameSourceMojoTest,
       ActivatedSourceTracksOnlyItsOriginalSourceId) {
  auto source = CreateSource();
  const BeginFrameArgs external_args =
      CreateBeginFrameArgsWithSourceId(/*source_id=*/123);
  source->IssueExternalBeginFrame(external_args, /*force=*/false,
                                  base::DoNothing());

  DidBeginFrame(
      CreateBeginFrameArgsWithSourceId(BeginFrameArgs::kStartingSourceId));
  EXPECT_TRUE(source->pending_frame_sinks_for_testing().empty());

  DidBeginFrame(external_args);
  EXPECT_TRUE(source->pending_frame_sinks_for_testing().contains(kFrameSinkId));
}

}  // namespace
}  // namespace viz
