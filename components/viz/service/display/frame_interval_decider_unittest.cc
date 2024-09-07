// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_interval_decider.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/flat_map.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "components/viz/service/surfaces/surface_manager_delegate.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "components/viz/test/stub_surface_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

using FixedIntervalSettings = FrameIntervalDecider::FixedIntervalSettings;
using FrameIntervalClass = FrameIntervalDecider::FrameIntervalClass;
using Result = FrameIntervalDecider::Result;

constexpr base::TimeTicks kNow = base::TimeTicks() + base::Seconds(1234);

void ExpectResult(Result result, FrameIntervalClass frame_interval_class) {
  ASSERT_TRUE(absl::holds_alternative<FrameIntervalClass>(result));
  EXPECT_EQ(frame_interval_class, absl::get<FrameIntervalClass>(result));
}

void ExpectResult(Result result, base::TimeDelta interval) {
  ASSERT_TRUE(absl::holds_alternative<base::TimeDelta>(result));
  EXPECT_EQ(interval, absl::get<base::TimeDelta>(result));
}

class TestFrameIntervalMatcher : public FrameIntervalMatcher {
 public:
  explicit TestFrameIntervalMatcher(FrameIntervalMatcherType type)
      : FrameIntervalMatcher(type) {}

  std::optional<Result> Match(const Inputs& matcher_inputs) override {
    last_matcher_inputs_.emplace(matcher_inputs);
    return result_;
  }

  bool has_last_matcher_inputs() const {
    return last_matcher_inputs_.has_value();
  }

  Inputs TakeLastMatcherInputs() {
    CHECK(last_matcher_inputs_);
    Inputs matcher_inputs = last_matcher_inputs_.value();
    last_matcher_inputs_.reset();
    return matcher_inputs;
  }

  void SetResult(std::optional<Result> result) { result_ = result; }

  base::WeakPtr<TestFrameIntervalMatcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::optional<Inputs> last_matcher_inputs_;
  std::optional<Result> result_;

  base::WeakPtrFactory<TestFrameIntervalMatcher> weak_ptr_factory_{this};
};

class FrameIntervalDeciderTest : public testing::Test,
                                 public SurfaceManagerDelegate {
 public:
  FrameIntervalDeciderTest() : frame_(MakeDefaultCompositorFrame()) {}
  ~FrameIntervalDeciderTest() override = default;

  void SetUp() override {
    surface_manager_ = std::make_unique<SurfaceManager>(
        this, /*activation_deadline_in_frames=*/std::nullopt,
        /*max_uncommitted_frames=*/0);
    decider_ = std::make_unique<FrameIntervalDecider>();
  }

  void TearDown() override {
    decider_.reset();
    surface_manager_.reset();
  }

  // SurfaceManagerDelegate implementation.
  std::string_view GetFrameSinkDebugLabel(
      const FrameSinkId& frame_sink_id) const override {
    return std::string_view();
  }
  void AggregatedFrameSinksChanged() override {}

 protected:
  base::WeakPtr<SurfaceClient> surface_client() {
    return surface_client_.weak_factory.GetWeakPtr();
  }

  Surface* CreateSurface(const FrameSinkId& frame_sink_id,
                         FrameIntervalInputs frame_interval_inputs) {
    LocalSurfaceId local_surface_id =
        LocalSurfaceId(1u, base::UnguessableToken::Create());
    SurfaceId surface_id(frame_sink_id, local_surface_id);
    SurfaceInfo surface_info(surface_id, frame_.device_scale_factor(),
                             frame_.size_in_pixels());
    Surface* surface = surface_manager_->CreateSurface(
        surface_client(), surface_info, SurfaceId());

    UpdateFrame(surface, std::move(frame_interval_inputs));

    return surface;
  }

  void UpdateFrame(Surface* surface,
                   FrameIntervalInputs frame_interval_inputs) {
    uint64_t frame_index = surface->GetActiveFrameIndex() + 1u;
    auto frame = MakeDefaultCompositorFrame();
    frame.metadata.frame_interval_inputs = std::move(frame_interval_inputs);
    ASSERT_TRUE(surface->QueueFrame(std::move(frame), frame_index,
                                    base::ScopedClosureRunner()));
    surface->ActivatePendingFrameForDeadline();
    ASSERT_EQ(surface->GetActiveFrameIndex(), frame_index);
  }

  void DrawSurfaces(std::vector<Surface*> surfaces,
                    base::TimeTicks frame_time) {
    std::unique_ptr<FrameIntervalDecider::ScopedAggregate> scoped_aggregate =
        decider_->WrapAggregate(*surface_manager_, frame_time);
    for (auto* surface : surfaces) {
      static_cast<SurfaceObserver*>(scoped_aggregate.get())
          ->OnSurfaceWillBeDrawn(surface);
    }
  }

  void InitializeDecider() {
    frame_.metadata.frame_interval_inputs.frame_time = kNow;

    std::vector<std::unique_ptr<FrameIntervalMatcher>> matchers;
    {
      auto matcher = std::make_unique<TestFrameIntervalMatcher>(
          FrameIntervalMatcherType::kInputBoost);
      matchers_.push_back(matcher->GetWeakPtr());
      matchers.push_back(std::move(matcher));
    }
    {
      auto matcher = std::make_unique<TestFrameIntervalMatcher>(
          FrameIntervalMatcherType::kOnlyVideo);
      matchers_.push_back(matcher->GetWeakPtr());
      matchers.push_back(std::move(matcher));
    }
    settings_.result_callback = base::BindRepeating(
        &FrameIntervalDeciderTest::SetFrameInterval, base::Unretained(this));
    decider_->UpdateSettings(settings_, std::move(matchers));
  }

  void SetFrameInterval(Result result, FrameIntervalMatcherType matcher_type) {
    result_ = result;
    matcher_type_ = matcher_type;
  }

  bool has_result() const { return result_.has_value(); }
  bool has_matcher_type() const { return matcher_type_.has_value(); }

  Result TakeLastResult() {
    CHECK(result_);
    Result result = result_.value();
    result_.reset();
    return result;
  }

  FrameIntervalMatcherType TakeLastMatcherType() {
    CHECK(matcher_type_);
    FrameIntervalMatcherType matcher_type = matcher_type_.value();
    matcher_type_.reset();
    return matcher_type;
  }

  CompositorFrame frame_;
  StubSurfaceClient surface_client_;

  FrameIntervalDecider::Settings settings_;
  // Matchers are probably owned by `decider_` so using WeakPtrs to catch
  // lifetime issues.
  std::vector<base::WeakPtr<TestFrameIntervalMatcher>> matchers_;

  std::unique_ptr<SurfaceManager> surface_manager_;
  std::unique_ptr<FrameIntervalDecider> decider_;

  // Set by `result_callback`.
  std::optional<Result> result_;
  std::optional<FrameIntervalMatcherType> matcher_type_;
};

TEST_F(FrameIntervalDeciderTest, Basics) {
  settings_.increase_frame_interval_timeout = base::Milliseconds(50);
  InitializeDecider();

  matchers_[0]->SetResult(FrameIntervalClass::kBoost);
  FrameIntervalInputs inputs;
  inputs.frame_time = kNow;
  Surface* surface1 = CreateSurface(FrameSinkId(0, 1), inputs);
  Surface* surface2 = CreateSurface(FrameSinkId(0, 2), inputs);
  DrawSurfaces({surface1, surface2}, kNow);

  ExpectResult(TakeLastResult(), FrameIntervalClass::kBoost);
  EXPECT_EQ(FrameIntervalMatcherType::kInputBoost, TakeLastMatcherType());
  FrameIntervalMatcher::Inputs inputs_from_matcher =
      matchers_[0]->TakeLastMatcherInputs();
  EXPECT_EQ(kNow, inputs_from_matcher.aggregated_frame_time);
  EXPECT_EQ(settings_.increase_frame_interval_timeout,
            inputs_from_matcher.settings->increase_frame_interval_timeout);
  EXPECT_EQ(2u, inputs_from_matcher.inputs_map.size());
  for (Surface* surface : std::vector<Surface*>{surface1, surface2}) {
    auto itr = inputs_from_matcher.inputs_map.find(
        surface->surface_id().frame_sink_id());
    ASSERT_NE(itr, inputs_from_matcher.inputs_map.end());

    EXPECT_EQ(surface->surface_id().frame_sink_id(), itr->first);
    EXPECT_EQ(kNow, itr->second.frame_time);
  }
  EXPECT_EQ(kNow, frame_.metadata.frame_interval_inputs.frame_time);

  EXPECT_FALSE(matchers_[1]->has_last_matcher_inputs());
}

TEST_F(FrameIntervalDeciderTest, NoMatch) {
  InitializeDecider();
  FrameIntervalInputs inputs;
  inputs.frame_time = kNow;
  Surface* surface = CreateSurface(FrameSinkId(0, 1), inputs);
  DrawSurfaces({surface}, kNow);

  // Check that matchers are consulted
  for (auto matcher : matchers_) {
    EXPECT_TRUE(matcher->has_last_matcher_inputs());
  }

  // Expect return kDefault if nothing matched.
  ExpectResult(TakeLastResult(), FrameIntervalClass::kDefault);
}

TEST_F(FrameIntervalDeciderTest, NoMatchFixedIntervals) {
  FixedIntervalSettings fixed_interval_settings;
  fixed_interval_settings.supported_intervals.insert(base::Milliseconds(8));
  fixed_interval_settings.supported_intervals.insert(base::Milliseconds(16));
  fixed_interval_settings.default_interval = base::Milliseconds(16);
  settings_.interval_settings = fixed_interval_settings;

  InitializeDecider();
  FrameIntervalInputs inputs;
  inputs.frame_time = kNow;
  Surface* surface = CreateSurface(FrameSinkId(0, 1), inputs);
  DrawSurfaces({surface}, kNow);

  EXPECT_TRUE(matchers_[1]->has_last_matcher_inputs());
  ExpectResult(TakeLastResult(), base::Milliseconds(16));
}

TEST_F(FrameIntervalDeciderTest, FirstMatch) {
  InitializeDecider();

  matchers_[1]->SetResult(base::Milliseconds(32));

  FrameIntervalInputs inputs;
  inputs.frame_time = kNow;
  DrawSurfaces({CreateSurface(FrameSinkId(0, 1), inputs)}, kNow);

  ExpectResult(TakeLastResult(), base::Milliseconds(32));
  EXPECT_EQ(FrameIntervalMatcherType::kOnlyVideo, TakeLastMatcherType());
  EXPECT_TRUE(matchers_[0]->has_last_matcher_inputs());
  EXPECT_TRUE(matchers_[1]->has_last_matcher_inputs());
}

TEST_F(FrameIntervalDeciderTest, NoChange) {
  InitializeDecider();

  matchers_[0]->SetResult(base::Milliseconds(32));

  FrameIntervalInputs inputs;
  inputs.frame_time = kNow;
  Surface* surface = CreateSurface(FrameSinkId(0, 1), inputs);
  DrawSurfaces({surface}, kNow);

  ExpectResult(TakeLastResult(), base::Milliseconds(32));
  EXPECT_EQ(FrameIntervalMatcherType::kInputBoost, TakeLastMatcherType());

  base::TimeTicks now2 = kNow + base::Milliseconds(16);
  inputs.frame_time = now2;
  UpdateFrame(surface, inputs);
  DrawSurfaces({surface}, now2);

  EXPECT_FALSE(has_result());
  EXPECT_FALSE(has_matcher_type());
}

TEST_F(FrameIntervalDeciderTest, IncreaseIntervalDelayFrameInterval) {
  settings_.increase_frame_interval_timeout = base::Milliseconds(60);
  InitializeDecider();

  base::TimeTicks now = kNow;
  matchers_[0]->SetResult(base::Milliseconds(32));

  FrameIntervalInputs inputs;
  inputs.frame_time = now;
  Surface* surface = CreateSurface(FrameSinkId(0, 1), inputs);
  DrawSurfaces({surface}, now);
  ExpectResult(TakeLastResult(), base::Milliseconds(32));
  EXPECT_EQ(FrameIntervalMatcherType::kInputBoost, TakeLastMatcherType());

  now = kNow + base::Milliseconds(16);
  matchers_[0]->SetResult(base::Milliseconds(16));
  DrawSurfaces({surface}, now);
  ExpectResult(TakeLastResult(), base::Milliseconds(16));
  EXPECT_EQ(FrameIntervalMatcherType::kInputBoost, TakeLastMatcherType());

  now = kNow + base::Milliseconds(32);
  matchers_[0]->SetResult(base::Milliseconds(32));
  DrawSurfaces({surface}, now);
  EXPECT_FALSE(has_result());
  EXPECT_FALSE(has_matcher_type());

  now = kNow + base::Milliseconds(96);
  DrawSurfaces({surface}, now);
  ExpectResult(TakeLastResult(), base::Milliseconds(32));
  EXPECT_EQ(FrameIntervalMatcherType::kInputBoost, TakeLastMatcherType());
}

TEST_F(FrameIntervalDeciderTest, IncreaseIntervalDelayIntervalClass) {
  settings_.increase_frame_interval_timeout = base::Milliseconds(60);
  InitializeDecider();

  base::TimeTicks now = kNow;
  matchers_[0]->SetResult(FrameIntervalClass::kDefault);

  FrameIntervalInputs inputs;
  inputs.frame_time = now;
  Surface* surface = CreateSurface(FrameSinkId(0, 1), inputs);
  DrawSurfaces({surface}, now);
  ExpectResult(TakeLastResult(), FrameIntervalClass::kDefault);
  EXPECT_EQ(FrameIntervalMatcherType::kInputBoost, TakeLastMatcherType());

  now = kNow + base::Milliseconds(16);
  matchers_[0]->SetResult(FrameIntervalClass::kBoost);
  DrawSurfaces({surface}, now);
  ExpectResult(TakeLastResult(), FrameIntervalClass::kBoost);
  EXPECT_EQ(FrameIntervalMatcherType::kInputBoost, TakeLastMatcherType());

  now = kNow + base::Milliseconds(32);
  matchers_[0]->SetResult(FrameIntervalClass::kDefault);
  DrawSurfaces({surface}, now);
  EXPECT_FALSE(has_result());
  EXPECT_FALSE(has_matcher_type());

  now = kNow + base::Milliseconds(96);
  DrawSurfaces({surface}, now);
  ExpectResult(TakeLastResult(), FrameIntervalClass::kDefault);
  EXPECT_EQ(FrameIntervalMatcherType::kInputBoost, TakeLastMatcherType());
}

TEST_F(FrameIntervalDeciderTest, IncreaseIntervalDelayVariantSwitch) {
  settings_.increase_frame_interval_timeout = base::Milliseconds(60);
  InitializeDecider();

  base::TimeTicks now = kNow;
  matchers_[0]->SetResult(FrameIntervalClass::kBoost);

  FrameIntervalInputs inputs;
  inputs.frame_time = now;
  Surface* surface = CreateSurface(FrameSinkId(0, 1), inputs);
  DrawSurfaces({surface}, now);
  ExpectResult(TakeLastResult(), FrameIntervalClass::kBoost);
  EXPECT_EQ(FrameIntervalMatcherType::kInputBoost, TakeLastMatcherType());

  now = kNow + base::Milliseconds(16);
  matchers_[0]->SetResult(base::Milliseconds(16));
  DrawSurfaces({surface}, now);
  ExpectResult(TakeLastResult(), base::Milliseconds(16));
  EXPECT_EQ(FrameIntervalMatcherType::kInputBoost, TakeLastMatcherType());

  now = kNow + base::Milliseconds(32);
  matchers_[0]->SetResult(FrameIntervalClass::kDefault);
  DrawSurfaces({surface}, now);
  ExpectResult(TakeLastResult(), FrameIntervalClass::kDefault);
  EXPECT_EQ(FrameIntervalMatcherType::kInputBoost, TakeLastMatcherType());

  now = kNow + base::Milliseconds(48);
  matchers_[0]->SetResult(base::Milliseconds(32));
  DrawSurfaces({surface}, now);
  ExpectResult(TakeLastResult(), base::Milliseconds(32));
  EXPECT_EQ(FrameIntervalMatcherType::kInputBoost, TakeLastMatcherType());
}

}  // namespace
}  // namespace viz
