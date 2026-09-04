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

// The "display won't draw" nak in MaybeProduceFrameCallback() must answer the
// in-flight IssueExternalBeginFrame() request with that request's frame id.
// It used to be built from `last_begin_frame_args_`, which is not updated when
// ExternalBeginFrameSource::OnBeginFrame() defers a frame while the GPU is
// busy (and is reset after every dispatched callback) — producing a nak with
// sequence number 0, which fails BeginFrameAck validation in the browser
// process and gets the GPU process terminated for a bad message.
TEST_F(ExternalBeginFrameSourceMojoTest,
       OriginalFrameIdTracksRequestWhenGpuBusyDefersBeginFrame) {
  auto source = CreateSource();
  source->SetIsGpuBusy(true);

  // Prime the busy-throttling state machine: the first BeginFrame while busy
  // is still delivered (and stored in `last_begin_frame_args_`), the second is
  // deferred without updating `last_begin_frame_args_`.
  source->OnBeginFrame(CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, /*source_id=*/123, /*sequence_number=*/5));
  source->OnBeginFrame(CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, /*source_id=*/123, /*sequence_number=*/6));
  EXPECT_EQ(5u, source->last_begin_frame_args().frame_id.sequence_number);

  // A request arriving in the deferred state must still be answerable with a
  // valid ack: `original_frame_id_` tracks the request even though
  // `last_begin_frame_args_` does not.
  const BeginFrameArgs request = CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, /*source_id=*/123, /*sequence_number=*/7);
  source->IssueExternalBeginFrame(request, base::DoNothing());

  ASSERT_TRUE(source->original_frame_id_for_testing().has_value());
  EXPECT_EQ(request.frame_id, *source->original_frame_id_for_testing());
  EXPECT_GE(source->original_frame_id_for_testing()->sequence_number,
            BeginFrameArgs::kStartingFrameNumber);
}

TEST_F(ExternalBeginFrameSourceMojoTest,
       ActivatedSourceTracksOnlyItsOriginalSourceId) {
  auto source = CreateSource();
  const BeginFrameArgs external_args =
      CreateBeginFrameArgsWithSourceId(/*source_id=*/123);
  source->IssueExternalBeginFrame(external_args, base::DoNothing());

  DidBeginFrame(
      CreateBeginFrameArgsWithSourceId(BeginFrameArgs::kStartingSourceId));
  EXPECT_TRUE(source->pending_frame_sinks_for_testing().empty());

  DidBeginFrame(external_args);
  EXPECT_TRUE(source->pending_frame_sinks_for_testing().contains(kFrameSinkId));
}

}  // namespace
}  // namespace viz
