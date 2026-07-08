// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MojoLPM fuzzer for the viz.mojom.CompositorFrameSink interface, exercised
// through a real CompositorFrameSinkSupport backed by a FrameSinkManagerImpl.
//
// The CompositorFrameSink interface is the renderer->Viz channel that carries
// the full CompositorFrame payload (render passes, draw quads, shared quad
// states, transferable resources, hit-test regions) into the GPU/Viz process.
// SubmitCompositorFrame is declared [EstimateSize, UnlimitedSize] in the mojom
// (services/viz/public/mojom/compositing/compositor_frame_sink.mojom), so the
// message-size validator does not bound the structure depth or width, making the
// deserialize (StructTraits::Read) and accept path (MaybeSubmitCompositorFrame /
// surface activation / resource refcounting) a high-value, previously-unfuzzed
// target reachable from a compromised renderer.
//
// Setup mirrors compositor_frame_sink_support_unittest.cc:
//   * FrameSinkManagerImpl(InitParams())
//   * SetSharedImageInterfaceProviderForTest(TestSharedImageInterfaceProvider)
//   * RegisterFrameSinkId(kFuzzFrameSinkId, report_activation=true)
//   * CompositorFrameSinkSupport(FakeCompositorFrameSinkClient, manager,
//                                kFuzzFrameSinkId, is_root=false)
//
// Frames are submitted through MaybeSubmitCompositorFrame() (which returns a
// SubmitResult) rather than the public SubmitCompositorFrame() wrapper (which
// DCHECK_EQ(result, SubmitResult::ACCEPTED) on a non-accepted result), so
// malformed fuzz frames are rejected cleanly instead of aborting the process.

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_mojolpm_fuzzer.pb.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/fake_compositor_frame_sink_client.h"
#include "components/viz/test/test_shared_image_interface_provider.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/tools/fuzzers/mojolpm.h"
#include "services/viz/public/mojom/compositing/begin_frame_args.mojom-mojolpm.h"
#include "services/viz/public/mojom/compositing/compositor_frame.mojom-mojolpm.h"
#include "services/viz/public/mojom/compositing/local_surface_id.mojom-mojolpm.h"
#include "services/viz/public/mojom/hit_test/hit_test_region_list.mojom-mojolpm.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

namespace {

namespace proto = viz::fuzzing::compositor_frame_sink::proto;

// Keep the structure bounded so a single testcase cannot OOM/timeout the
// fuzzer; the interesting deserialize/activation bugs reproduce at small N.
// These mirror the spirit of layer_context_impl_mojolpm_fuzzer.cc's clamps.
constexpr int kMaxRenderPasses = 50;
constexpr int kMaxQuadsPerPass = 200;
constexpr int kMaxResources = 200;

// Fixed sink identity (matches the unittest's arbitrary FrameSinkId(1, 1)).
constexpr bool kIsRoot = false;
const viz::FrameSinkId kFuzzFrameSinkId(1, 1);

struct FuzzerEnvironment {
  FuzzerEnvironment() {
    mojo::core::Init();
  }
  // base::AtExitManager must be the first member and live for the whole process;
  // anything reaching a base/ at-exit registration CHECKs without it.
  base::AtExitManager at_exit_manager;
  // TestTimeouts::Initialize() MUST run before the TaskEnvironment member is
  // constructed (its ctor DCHECKs TestTimeouts is initialized). A member ctor
  // runs before the enclosing ctor body, so do it in a lambda-init member that
  // is declared *before* task_environment.
  const bool initialized_ = [] {
    base::CommandLine::Init(0, nullptr);
    TestTimeouts::Initialize();
    return true;
  }();
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

// Proto-level shaping.
//
// We shape the mojolpm proto before FromProto materializes the (hard to
// truncate) C++ ListContainer-backed QuadList / CompositorRenderPassList. We do
// two things:
//   (1) Clamp the repeated fields so a single testcase stays bounded.
//   (2) "Nudge" the begin_frame_ack sequence number to a valid value when (and
//       only when) it is absent or zero, so that an otherwise-well-formed
//       mutated frame can clear the IsSequenceValid() early-reject in
//       CompositorFrameSinkSupport::MaybeSubmitCompositorFrame() and reach the
//       deeper service state machine (surface creation/activation, resource
//       refcounting). Mutation can still set a nonzero-but-otherwise-invalid
//       ack, so the reject paths remain reachable; we only fill in a default
//       when empty.

// Ensure a struct-typed mojolpm union is in its "new" (freshly-constructed)
// arm so that mutable_new_() is valid to call. If the union is unset we create
// the "new" arm and return it. If it is already a "new" arm we return it as-is.
// If it is in a back-reference ("old") arm, a reference to a previously seen
// instance that we must not rewrite, we return nullptr.
template <typename ProtoUnion>
auto* AsFreshOrNull(ProtoUnion* u) {
  if (u->instance_case() == ProtoUnion::INSTANCE_NOT_SET) {
    // mutable_new_() switches the union to the "new" arm and returns it; bail
    // out here rather than falling through, since the union is now "new".
    return u->mutable_new_();
  }
  if (u->instance_case() != ProtoUnion::kNew) {
    return decltype(u->mutable_new_()){nullptr};
  }
  return u->mutable_new_();
}

void ClampResources(
    mojolpm::viz::mojom::CompositorFrame_ProtoStruct* new_frame) {
  if (!new_frame->has_m_resources()) {
    return;
  }
  auto* resources = new_frame->mutable_m_resources();
  while (resources->values_size() > kMaxResources) {
    resources->mutable_values()->RemoveLast();
  }
}

void ClampPasses(mojolpm::viz::mojom::CompositorFrame_ProtoStruct* new_frame) {
  if (!new_frame->has_m_passes()) {
    return;
  }
  auto* passes = new_frame->mutable_m_passes();
  while (passes->values_size() > kMaxRenderPasses) {
    passes->mutable_values()->RemoveLast();
  }
  for (int i = 0; i < passes->values_size(); ++i) {
    auto* pass_holder = passes->mutable_values(i);
    if (!pass_holder->has_value()) {
      continue;
    }
    auto* pass = pass_holder->mutable_value();
    if (pass->instance_case() !=
        mojolpm::viz::mojom::CompositorRenderPass::kNew) {
      continue;
    }
    auto* new_pass = pass->mutable_new_();
    if (!new_pass->has_m_quad_list()) {
      continue;
    }
    auto* quads = new_pass->mutable_m_quad_list();
    while (quads->values_size() > kMaxQuadsPerPass) {
      quads->mutable_values()->RemoveLast();
    }
    // No separate shared_quad_state_list clamp is needed. In the mojom,
    // CompositorRenderPass carries only `quad_list`; each DrawQuad holds an
    // optional inline `sqs`, and CompositorRenderPassStructTraits::Read rebuilds
    // the C++ shared_quad_state_list by appending at most one SharedQuadState
    // per quad that carries an inline sqs. So bounding the quad count above also
    // bounds the SharedQuadState count; there is no independent
    // fuzzer-controlled SQS array that could allocate unboundedly.
  }
}

// Fill an absent/zero begin_frame_ack.sequence_number with a valid value so a
// well-formed frame reaches the service state machine. Only touches the field
// when it is empty/zero; leaves mutator-chosen nonzero values alone.
//
// On the mojom/C++ shape mismatch:
//   * In the .mojom (and hence the mojolpm proto) viz.mojom.BeginFrameAck has
//     flat fields {source_id, sequence_number, trace_id, has_damage}, with no
//     nested frame_id struct.
//   * BeginFrameAckStructTraits maps mojom `sequence_number` to
//     C++ ack.frame_id.sequence_number.
//   * IsSequenceValid() at the CompositorFrameSinkSupport service gate requires
//     ack.frame_id.sequence_number != BeginFrameArgs::kInvalidFrameNumber
//     (==0).
// So we set the proto's flat m_sequence_number on the ack rather than a nested
// frame_id.
void NudgeBeginFrameAck(
    mojolpm::viz::mojom::CompositorFrame_ProtoStruct* new_frame) {
  if (!new_frame->has_m_metadata()) {
    return;
  }
  auto* metadata = AsFreshOrNull(new_frame->mutable_m_metadata());
  if (!metadata || !metadata->has_m_begin_frame_ack()) {
    return;
  }
  auto* ack = AsFreshOrNull(metadata->mutable_m_begin_frame_ack());
  if (!ack) {
    return;
  }
  if (!ack->has_m_sequence_number() || ack->m_sequence_number() == 0) {
    ack->set_m_sequence_number(1);
  }
}

void ShapeCompositorFrame(mojolpm::viz::mojom::CompositorFrame* frame) {
  // We always want a fresh CompositorFrame built from the fuzzer input. The
  // generated CompositorFrame proto is a union of an "old" arm (a uint32
  // back-reference into mojolpm's instance registry of previously deserialized
  // frames) and a "new" arm (a fully inline struct). Reusing a prior frame by
  // id is not meaningful for this single-sink harness, so unconditionally
  // switch the union to its "new" arm: mutable_new_() clears any "old"
  // back-reference and returns the inline struct that drives a real
  // SubmitCompositorFrame. (If the input was already "new", its fields are
  // preserved.)
  auto* new_frame = frame->mutable_new_();
  ClampResources(new_frame);
  ClampPasses(new_frame);
  NudgeBeginFrameAck(new_frame);
}

// Service stack, mirroring CompositorFrameSinkSupportTestBase::SetUp().
class FrameSinkFuzzerHarness {
 public:
  FrameSinkFuzzerHarness() {
    manager_ = std::make_unique<viz::FrameSinkManagerImpl>(
        viz::FrameSinkManagerImpl::InitParams());
    // Must be set before any CompositorFrameSinkSupport is created/used so the
    // resource path has a (stub) shared-image backend.
    manager_->SetSharedImageInterfaceProviderForTest(
        &shared_image_interface_provider_);
    manager_->RegisterFrameSinkId(kFuzzFrameSinkId,
                                  /*report_activation=*/true);
    support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
        &fake_client_, manager_.get(), kFuzzFrameSinkId, kIsRoot);
  }

  ~FrameSinkFuzzerHarness() {
    support_.reset();
    // InvalidateFrameSinkId(FrameSinkId) => (); the callback is OnceClosure.
    manager_->InvalidateFrameSinkId(kFuzzFrameSinkId, base::DoNothing());
    manager_.reset();
  }

  viz::CompositorFrameSinkSupport* support() { return support_.get(); }

 private:
  viz::TestSharedImageInterfaceProvider shared_image_interface_provider_;
  std::unique_ptr<viz::FrameSinkManagerImpl> manager_;
  viz::FakeCompositorFrameSinkClient fake_client_;
  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;
};

class CompositorFrameSinkTestcase
    : public mojolpm::Testcase<proto::Testcase, proto::Action> {
 public:
  explicit CompositorFrameSinkTestcase(const proto::Testcase& testcase)
      : mojolpm::Testcase<proto::Testcase, proto::Action>(testcase) {}

  void SetUp(base::OnceClosure done_closure) override {
    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
  }

  void TearDown(base::OnceClosure done_closure) override {
    harness_.reset();
    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
  }

  void RunAction(const proto::Action& action,
                 base::OnceClosure run_closure) override {
    auto* support = harness_->support();
    switch (action.action_case()) {
      case proto::Action::kRunThread:
        // Pump posted continuations (acks, surface activation, GC) so they run
        // between actions rather than only at teardown.
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTaskAndReply(
            FROM_HERE, base::DoNothing(), std::move(run_closure));
        return;

      case proto::Action::kSubmitCompositorFrame: {
        const auto& a = action.submit_compositor_frame();

        viz::LocalSurfaceId local_surface_id;
        if (!mojolpm::FromProto(a.local_surface_id(), local_surface_id)) {
          break;
        }

        // Copy + shape the proto before deserializing into C++ types.
        auto proto_frame = a.frame();
        ShapeCompositorFrame(&proto_frame);
        viz::CompositorFrame frame;
        if (!mojolpm::FromProto(proto_frame, frame)) {
          // Deserialization rejected the (possibly malformed) graph; this is
          // itself a meaningful coverage point in the StructTraits::Read path.
          break;
        }

        std::optional<viz::HitTestRegionList> hit_test_region_list;
        if (a.has_hit_test_region_list()) {
          viz::HitTestRegionList list;
          if (mojolpm::FromProto(a.hit_test_region_list(), list)) {
            hit_test_region_list = std::move(list);
          }
        }

        // Use MaybeSubmitCompositorFrame so a rejected frame returns a
        // SubmitResult rather than DCHECK-failing (the public wrapper
        // DCHECK_EQ()s ACCEPTED).
        std::ignore = support->MaybeSubmitCompositorFrame(
            local_surface_id, std::move(frame), std::move(hit_test_region_list),
            a.has_submit_time() ? a.submit_time() : 0u);
        break;
      }

      case proto::Action::kDidNotProduceFrame: {
        viz::BeginFrameAck ack;
        if (mojolpm::FromProto(action.did_not_produce_frame().ack(), ack)) {
          std::ignore = support->DidNotProduceFrame(ack);
        }
        break;
      }

      case proto::Action::kSetNeedsBeginFrame:
        support->SetNeedsBeginFrame(
            action.set_needs_begin_frame().needs_begin_frame());
        break;

      case proto::Action::kNotifyNewLocalSurfaceIdExpectedWhilePaused:
        support->NotifyNewLocalSurfaceIdExpectedWhilePaused();
        break;

      case proto::Action::kSetWantsAnimateOnlyBeginFrames:
        support->SetWantsAnimateOnlyBeginFrames();
        break;

      case proto::Action::ACTION_NOT_SET:
        break;
    }

    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
  }

 private:
  std::unique_ptr<FrameSinkFuzzerHarness> harness_ =
      std::make_unique<FrameSinkFuzzerHarness>();
};

}  // namespace

DEFINE_BINARY_PROTO_FUZZER(const proto::Testcase& testcase) {
  // Execution is sequence-driven: sequence_indexes[i] -> sequences[that] ->
  // action_indexes[j] -> actions[that]. mojolpm::Testcase::IsFinished() bails
  // before running any action unless actions, sequences AND sequence_indexes
  // are all non-empty, so require all three here (a loose
  // "!actions_size() && !sequences_size()" guard would let dead inputs
  // through).
  if (!testcase.actions_size() || !testcase.sequences_size() ||
      !testcase.sequence_indexes_size()) {
    return;
  }

  static base::NoDestructor<FuzzerEnvironment> env;

  CompositorFrameSinkTestcase testcase_runner(testcase);

  base::RunLoop main_run_loop;
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&mojolpm::RunTestcase<CompositorFrameSinkTestcase>,
                     base::Unretained(&testcase_runner), GetFuzzerTaskRunner(),
                     main_run_loop.QuitClosure()));
  main_run_loop.Run();
}
