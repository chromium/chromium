// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/animation/browser_animation_controller.h"

#include <algorithm>
#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "base/callback_list.h"
#include "base/check_is_test.h"
#include "base/containers/adapters.h"
#include "base/containers/map_util.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ui/animation/browser_animation_provider.h"
#include "chrome/browser/ui/animation/browser_animation_provider_internal.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/widget/widget.h"

namespace {

using Transition = internal::BrowserAnimationTransition;
using AnimationValue = internal::BrowserAnimationValue;
using ValueLookup = std::map<BrowserAnimationSequence, AnimationValue>;
using KeyframeValueType = internal::BrowserAnimationKeyframe::ValueType;
using KeyframeValue = internal::BrowserAnimationKeyframe::Value;

// Stores sequence parameters for easy retrieval.
class SequenceParamsLookup {
 public:
  explicit SequenceParamsLookup(
      internal::BrowserAnimationSequenceParamsLookup lookup)
      : lookup_(std::move(lookup)) {}
  ~SequenceParamsLookup() = default;

  // Gets the default value for `sequence` or null if none.
  std::optional<double> GetDefaultValue(
      BrowserAnimationSequence sequence) const {
    const auto* const value = base::FindOrNull(lookup_, sequence);
    return value ? value->default_value : std::nullopt;
  }

  // Gets whether `sequence` should persist.
  bool ShouldPersist(BrowserAnimationSequence sequence) const {
    auto it = lookup_.find(sequence);
    if (it == lookup_.end()) {
      return false;
    }
    return it->second.persist_between_animations;
  }

 private:
  internal::BrowserAnimationSequenceParamsLookup lookup_;
};

// Class that records an FPS histogram with name starting with `prefix` for an
// animation if it completes successfully.
class Logger : public ui::CompositorObserver {
 public:
  Logger(views::Widget* widget,
         base::TimeDelta animation_length,
         std::string_view prefix)
      : animation_length_(animation_length),
        prefix_(prefix),
        last_frame_(base::Time::Now()) {
    if (widget && widget->GetCompositor()) {
      CHECK(animation_length_.is_positive());
      CHECK(!prefix.empty());
      compositor_observation_.Observe(widget->GetCompositor());
    }
  }
  Logger(const Logger&) = delete;
  void operator=(const Logger&) = delete;
  ~Logger() override = default;

  void MaybeLog() {
    if (!compositor_observation_.IsObserving()) {
      return;
    }

    // On very slow machines, it is possible for an animation to have zero
    // frames presented. We need to record these (very bad) results. So if there
    // have been zero frames, assume that there will be a frame soon after and
    // count this call as a frame..
    //
    // This does result in the FPS bottoming out at `1 / animation_length_`, and
    // the longest frame at `animation_length_`, but those are already worst-
    // case scenarios.
    if (frames_presented_ == 0U) {
      frames_presented_ = 1U;
      longest_frame_ =
          std::max(longest_frame_, base::Time::Now() - last_frame_);
    }

    compositor_observation_.Reset();
    const int animation_fps =
        base::ClampRound(frames_presented_ / animation_length_.InSecondsF());
    base::UmaHistogramCounts100(
        prefix_ + BrowserAnimationController::kFramesPerSecondHistogramSuffix,
        animation_fps);
    base::UmaHistogramTimes(
        prefix_ + BrowserAnimationController::kLongestFrameHistogramSuffix,
        longest_frame_);
  }

 private:
  // ui::CompositorObserver:
  void OnDidPresentCompositorFrame(ui::Compositor*,
                                   uint32_t,
                                   const gfx::PresentationFeedback&) override {
    ++frames_presented_;
    const auto now = base::Time::Now();
    longest_frame_ = std::max(longest_frame_, now - last_frame_);
    last_frame_ = now;
  }
  void OnCompositingShuttingDown(ui::Compositor* compositor) override {
    compositor_observation_.Reset();
  }

  const base::TimeDelta animation_length_;
  const std::string prefix_;
  size_t frames_presented_ = 0U;
  base::Time last_frame_;
  base::TimeDelta longest_frame_;
  base::ScopedObservation<ui::Compositor, ui::CompositorObserver>
      compositor_observation_{this};
};

// Class that creates an on-demand views::AnimationDelegateViews.
//
// This allows us to set up and query group data without having to create a
// compositor-based animation container before the browser view is initialized.
class Animator : public views::AnimationDelegateViews {
 public:
  explicit Animator(gfx::AnimationDelegate& delegate, views::View* view)
      : AnimationDelegateViews(view), delegate_(delegate) {}
  ~Animator() override = default;

  void Start(base::TimeDelta duration) {
    animation_.SetDuration(duration);
    animation_.Start();
  }

  void Stop() { animation_.Stop(); }

  void SetAnimationContainerForTesting(gfx::AnimationContainer* container) {
    animation_.SetContainer(container);
  }

  // views::AnimationDelegateViews:
  void AnimationEnded(const gfx::Animation* animation) override {
    delegate_->AnimationEnded(animation);
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    delegate_->AnimationProgressed(animation);
  }

 private:
  raw_ref<gfx::AnimationDelegate> delegate_;
  gfx::LinearAnimation animation_{this};
};

// Similar to `base::FindOrNull()`, but returns a `std::optional` instead of a
// pointer.
template <typename K, typename V, typename C>
std::optional<V> FindOrNullopt(const std::map<K, V, C>& map, K key) {
  const auto it = map.find(key);
  if (it == map.end()) {
    return std::nullopt;
  }
  return it->second;
}

// Resolves `value` to a concrete numeric value using `default_value` if
// necessary. It is an error if `value` requires a default but `default_value`
// is null.
double ResolveToValue(const KeyframeValue& value,
                      const std::optional<double>& default_value) {
  switch (value.type) {
    case KeyframeValueType::kDefault:
      CHECK(default_value)
          << "Cannot resolve value because value is default but "
             "there is no default value.";
      return *default_value;
    case KeyframeValueType::kConcrete:
      return value.value;
    case KeyframeValueType::kMaxOfDefaultAnd:
      CHECK(default_value)
          << "Cannot resolve value because value is default but "
             "there is no default value.";
      return std::max(value.value, *default_value);
    case KeyframeValueType::kMinOfDefaultAnd:
      CHECK(default_value)
          << "Cannot resolve value because value is default but "
             "there is no default value.";
      return std::min(value.value, *default_value);
  }
}

// Resolves `value` to a `KeyframeValue` (i.e. either a concrete value or
// default) using `default_value` if necessary. It is an error if `value`
// requires a default but `default_value` is null.
AnimationValue ResolveToAnimationValue(
    const KeyframeValue& value,
    const std::optional<double>& default_value) {
  switch (value.type) {
    case KeyframeValueType::kDefault:
      return internal::DefaultBrowserAnimationValue();
    case KeyframeValueType::kConcrete:
      return value.value;
    case KeyframeValueType::kMaxOfDefaultAnd:
      CHECK(default_value)
          << "Cannot resolve value because value is default but "
             "there is no default value.";
      return value.value > *default_value
                 ? AnimationValue(value.value)
                 : internal::DefaultBrowserAnimationValue();
    case KeyframeValueType::kMinOfDefaultAnd:
      CHECK(default_value)
          << "Cannot resolve value because value is default but "
             "there is no default value.";
      return value.value < *default_value
                 ? AnimationValue(value.value)
                 : internal::DefaultBrowserAnimationValue();
  }
}

// Resolves `value` to a concrete numeric value using `default_value` if
// necessary. It is an error if `value` is default but `default_value` is null.
double Resolve(const AnimationValue& value,
               const std::optional<double>& default_value) {
  if (std::holds_alternative<internal::DefaultBrowserAnimationValue>(value)) {
    CHECK(default_value) << "Cannot resolve value because value is default but "
                            "there is no default value.";
    return *default_value;
  }
  return std::get<double>(value);
}

// Possibly applies a transition for a value in `sequence`.
//
// The `current` is the raw value of the sequence based on its keyframes, while
// `persisted` is the value prior to the animation starting.
AnimationValue MaybeApplyTransition(
    const internal::BrowserAnimationSequenceSpecification& sequence,
    Transition transition,
    const AnimationValue& current,
    const AnimationValue& persisted,
    double progress_percent,
    const std::optional<double>& default_value) {
  const auto initial = sequence.start();
  const auto final = sequence.end();
  const double current_value = Resolve(current, default_value);
  const double start_value = Resolve(persisted, default_value);
  const double initial_value = ResolveToValue(initial, default_value);
  const double final_value = ResolveToValue(final, default_value);

  // Ignore the transition if the values are functionally equivalent.
  if (start_value == initial_value ||
      transition == Transition::kIgnoreOldValue) {
    return current;
  }

  // If the starting value is outside [initial, final], tween the value between
  // the start and current value over the course of the animation.
  if (start_value < std::min(initial_value, final_value)) {
    // Ensure that we can return default values if at either end of the
    // transition.
    if (progress_percent == 0.0) {
      return persisted;
    }
    if (progress_percent == 1.0) {
      return current;
    }
    const double adjusted = current_value + (1.0 - progress_percent) *
                                                (start_value - current_value);
    return std::min(current_value, adjusted);
  }
  if (start_value > std::max(initial_value, final_value)) {
    // Ensure that we can return default values if at either end of the
    // transition.
    if (progress_percent == 0.0) {
      return persisted;
    }
    if (progress_percent == 1.0) {
      return current;
    }
    const double adjusted = current_value + (1.0 - progress_percent) *
                                                (start_value - current_value);
    return std::max(current_value, adjusted);
  }

  // For caps, set the cap based on whether the animation is increasing or
  // decreasing.
  if (transition == Transition::kCapAtOldValue) {
    if (initial_value <= final_value) {
      return std::max(start_value, current_value);
    }
    return std::min(start_value, current_value);
  }

  CHECK_EQ(transition, Transition::kStartAtOldValue);

  // This is a trivial scaling; return the current value to avoid
  // divide-by-zero.
  CHECK_NE(initial_value, final_value)
      << "If initial and final were the same, and start is not outside those "
         "values, then start would be equal to initial, which we handled "
         "above.";

  // Ensure that we can return default values if at either end of the
  // transition.
  if (progress_percent == 0.0) {
    return persisted;
  }
  if (progress_percent == 1.0) {
    return current;
  }

  // Shrink the range of the motion based on how close to the final value
  // `start` is.
  const double scale =
      std::abs((start_value - final_value) / (initial_value - final_value));
  return scale * (current_value - final_value) + final_value;
}

// Computes the current value of `sequence` at `progress`, which can be a time
// delta or a progress percentage (the logic for the two is similar enough that
// they are combined into a single function).
template <typename T>
AnimationValue GetSequenceValueT(
    const internal::BrowserAnimationSequenceSpecification& sequence,
    T progress,
    const std::optional<double>& default_value) {
  // Search for the first frame on or after `progress`.
  const internal::BrowserAnimationKeyframe* prev_frame = nullptr;
  T prev_frame_time = T();
  for (const auto& frame : sequence.keyframes) {
    // Compute the progress on this `frame`.
    T frame_time;
    if constexpr (std::same_as<T, double>) {
      frame_time = frame.time.percent().value_or(0.0);
    } else {
      frame_time = frame.time.absolute_time().value_or(base::Milliseconds(0));
    }

    // If exactly on a frame, use that frame value.
    if (progress == frame_time || (!prev_frame && progress < frame_time)) {
      return ResolveToAnimationValue(frame.value, default_value);
    }

    // If between frames, calculate the value on the curve between the previous
    // and current keyframes.
    if (progress < frame_time) {
      const double percent =
          (progress - prev_frame_time) / (frame_time - prev_frame_time);
      const double tween_value =
          gfx::Tween::CalculateValue(frame.tween_type, percent);
      const double tween_start =
          ResolveToValue(prev_frame->value, default_value);
      const double tween_end = ResolveToValue(frame.value, default_value);
      const double result =
          tween_start + (tween_end - tween_start) * tween_value;
      // If adjacent keyframes can use defaults, and the value is the default,
      // then return an explicit default.
      if ((prev_frame->value.type != KeyframeValueType::kConcrete ||
           frame.value.type != KeyframeValueType::kConcrete) &&
          result == *default_value) {
        return internal::DefaultBrowserAnimationValue();
      }
      return result;
    }

    // Still ahead of the current frame, so advance.
    prev_frame = &frame;
    prev_frame_time = frame_time;
  }

  // If past the final frame, use its value.
  return ResolveToAnimationValue(prev_frame->value, default_value);
}

// Calculates the value for `sequence` during an animation.
AnimationValue GetSequenceValue(
    const internal::BrowserAnimationSequenceSpecification& sequence,
    base::TimeDelta absolute_time,
    double scaled_percent,
    const std::optional<double>& default_value,
    const std::optional<AnimationValue>& persisted,
    Transition transition) {
  // Calculate the raw value of the curve at the current progress.
  AnimationValue result;
  if (sequence.keyframes.back().time.percent()) {
    result = GetSequenceValueT(sequence, scaled_percent, default_value);
  } else if (sequence.keyframes.back().time.absolute_time()) {
    result = GetSequenceValueT(sequence, absolute_time, default_value);
  } else {
    result = ResolveToAnimationValue(sequence.start(), default_value);
  }

  // If we are redirecting from another animation or value, apply the
  // transition to the raw value.
  if (persisted) {
    result = MaybeApplyTransition(sequence, transition, result, *persisted,
                                  scaled_percent, default_value);
  }
  return result;
}

// Calculates the value for a `Return()` type sequence.
AnimationValue GetReturnToValue(
    const internal::BrowserAnimationSequenceSpecification& sequence,
    double scaled_percent,
    const std::optional<double>& default_value,
    const std::optional<AnimationValue>& persisted_value,
    Transition transition) {
  // There should only be one keyframe for return-to sequences.
  const auto& keyframe = sequence.keyframes.back();
  CHECK_EQ(1.0, *keyframe.time.percent());
  CHECK_EQ(Transition::kStartAtOldValue, transition);

  const auto keyframe_as_animation_value =
      ResolveToAnimationValue(keyframe.value, default_value);

  // If there's nothing to transition, just use the target.
  if (scaled_percent == 1.0 || !persisted_value ||
      persisted_value.value() == keyframe_as_animation_value) {
    return keyframe_as_animation_value;
  }

  // Likewise, no calculation is needed at the start.
  if (scaled_percent == 0.0) {
    return *persisted_value;
  }

  // Perform a tween between persisted and target values.
  return gfx::Tween::DoubleValueBetween(
      gfx::Tween::CalculateValue(keyframe.tween_type, scaled_percent),
      Resolve(*persisted_value, default_value),
      ResolveToValue(keyframe.value, default_value));
}

}  // namespace

DEFINE_USER_DATA(BrowserAnimationController);

struct BrowserAnimationController::MotionInfo {
  MotionInfo() = default;
  MotionInfo(MotionInfo&&) noexcept = default;
  MotionInfo& operator=(MotionInfo&&) noexcept = default;
  ~MotionInfo() = default;

  internal::BrowserAnimationMotionSpecification motion;
  base::TimeDelta duration;
  BrowserAnimationProvider::HistogramPrefix histogram_prefix;
};

// Tracks the current motion and progress for a particular animation group.
class BrowserAnimationController::GroupData : public gfx::AnimationDelegate {
 public:
  GroupData(const BrowserAnimationController& controller,
            BrowserAnimationGroup group)
      : controller_(controller), group_(group) {}

  ~GroupData() override = default;

  // Starts `motion` for the current group. Any existing motion will be
  // canceled.
  void Start(BrowserAnimationMotion motion, MotionInfo motion_info) {
    const auto params = GetParamsLookup();
    OnCancel(params);
    current_motion_ = motion;
    current_motion_info_ = std::move(motion_info);
    UpdateCurrentValues(base::Milliseconds(0), 0.0, params);
    if (gfx::Animation::ShouldRenderRichAnimation()) {
      MaybeCreateAnimator();
      MaybeStartLogger();
      animator_->Start(current_motion_info_.duration);
      Notify(BrowserAnimationUpdate::kStarted);
    } else {
      Notify(BrowserAnimationUpdate::kStarted);
      OnAnimationEnded(params);
    }
  }

  // Resets the animation for the current group to the end of `motion`.
  void Reset(BrowserAnimationMotion motion, MotionInfo motion_info) {
    const auto params = GetParamsLookup();
    if (current_motion_ != motion) {
      OnCancel(params);
      current_motion_ = motion;
      current_motion_info_ = std::move(motion_info);
    }
    OnAnimationEnded(params);
  }

  // Cancels or clears the animations for the current group.
  bool Cancel(bool clear) {
    return clear ? OnClear() : OnCancel(GetParamsLookup());
  }

  // Retrieves the current value for `sequence` in this group, or null if none.
  // Default values are returned as null; the caller should apply the actual
  // default.
  std::optional<double> GetCurrentValue(
      BrowserAnimationSequence sequence) const {
    const auto* value = base::FindOrNull(current_values_, sequence);
    if (!value) {
      value = base::FindOrNull(persisted_values_, sequence);
    }
    if (value) {
      if (const double* const concrete_value = std::get_if<double>(value)) {
        return *concrete_value;
      }
    }
    return std::nullopt;
  }

  // Returns the current motion duration.
  base::TimeDelta GetMotionDuration() const {
    if (!current_motion_) {
      return base::Seconds(0);
    }
    return current_motion_info_.duration;
  }

  // Subscribes to updates for this group.
  base::CallbackListSubscription Subscribe(BrowserAnimationCallback callback) {
    return callbacks_.Add(std::move(callback));
  }

  // Used in tests to override the default animation container, typically to one
  // which can be directly manipulated.
  void SetAnimationContainerForTesting(gfx::AnimationContainer* container) {
    MaybeCreateAnimator();
    animator_->SetAnimationContainerForTesting(container);  // IN-TEST
  }

  BrowserAnimationMotion current_motion() const { return current_motion_; }

 private:
  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation*) override {
    const auto params = GetParamsLookup();
    OnAnimationEnded(params);
  }
  void AnimationProgressed(const gfx::Animation* animation) override {
    const auto params = GetParamsLookup();
    const double unscaled_percent = animation->GetCurrentValue();
    OnAnimationProgressed(unscaled_percent, params);
  }

  // Creates an animator if one does not exist.
  void MaybeCreateAnimator() {
    if (!animator_) {
      views::View* const view = controller_->browser_view_;
      if (!view) {
        CHECK_IS_TEST()
            << "Can only attempt to start animations without a view in tests.";
      }
      animator_.emplace(*this, view);
    }
  }

  SequenceParamsLookup GetParamsLookup() const {
    return SequenceParamsLookup(controller_->GetAllSequenceParams(group_));
  }

  // Starts the logger for the current motion if it is nonzero duration, has a
  // log prefix/name, and there is a widget to get the compositor from.
  void MaybeStartLogger() {
    CHECK(current_motion_);
    if (current_motion_info_.histogram_prefix.is_specified() &&
        current_motion_info_.duration.is_positive()) {
      if (auto* const widget = controller_->browser_view_->GetWidget()) {
        logger_.emplace(widget, current_motion_info_.duration,
                        current_motion_info_.histogram_prefix.GetFullPrefix());
      }
    }
  }

  // Does the work of progressing the animation. Values are updated and then
  // listeners are notified.
  void OnAnimationProgressed(double unscaled_percent,
                             const SequenceParamsLookup& params) {
    const double scaled_percent = gfx::Tween::CalculateValue(
        current_motion_info_.motion.global_tween, unscaled_percent);
    const base::TimeDelta absolute_time =
        current_motion_info_.duration * unscaled_percent;
    UpdateCurrentValues(absolute_time, scaled_percent, params);
    Notify(BrowserAnimationUpdate::kProgressed);
  }

  // Does the work of ending an animation. Listeners will be updated with the
  // animation active at 100% before the animation is cleaned up.
  void OnAnimationEnded(const SequenceParamsLookup& params) {
    UpdateCurrentValues(current_motion_info_.duration, 1.0, params);
    if (logger_) {
      logger_->MaybeLog();
      logger_.reset();
    }
    Notify(BrowserAnimationUpdate::kEnded);
    current_motion_ = BrowserAnimationMotion();
    PersistCurrentValues(params);
  }

  // Does the work of canceling the current animation.
  bool OnCancel(const SequenceParamsLookup& params) {
    if (!current_motion_) {
      return false;
    }
    PersistCurrentValues(params);
    logger_.reset();
    animator_->Stop();
    Notify(BrowserAnimationUpdate::kCanceled);
    current_motion_ = BrowserAnimationMotion();
    return true;
  }

  // Does the work of clearing out all animation data.
  bool OnClear() {
    current_values_.clear();
    persisted_values_.clear();
    if (!current_motion_) {
      return false;
    }
    logger_.reset();
    animator_->Stop();
    Notify(BrowserAnimationUpdate::kCanceled);
    current_motion_ = BrowserAnimationMotion();
    return true;
  }

  // Calculates all current values based on the current time and progress
  // through the animation.
  void UpdateCurrentValues(base::TimeDelta absolute_time,
                           double scaled_percent,
                           const SequenceParamsLookup& params) {
    CHECK(current_motion_);
    for (const auto& [sequence, sequence_spec] :
         current_motion_info_.motion.sequences) {
      const auto persisted = FindOrNullopt(persisted_values_, sequence);
      const auto default_value = params.GetDefaultValue(sequence);
      if (sequence_spec.is_return_to()) {
        current_values_[sequence] =
            GetReturnToValue(sequence_spec, scaled_percent, default_value,
                             persisted, sequence_spec.transition);
        continue;
      }
      current_values_[sequence] =
          GetSequenceValue(sequence_spec, absolute_time, scaled_percent,
                           default_value, persisted, sequence_spec.transition);
    }
  }

  // Notifies all listeners that `update` has happened.
  void Notify(BrowserAnimationUpdate update) {
    callbacks_.Notify(&controller_.get(), update);
  }

  // Persists any of the current values that should be persisted. Called when an
  // animation ends or is canceled. Persisted values are used for transitions
  // and in place of current values between animations.
  void PersistCurrentValues(const SequenceParamsLookup& params) {
    ValueLookup temp;

    // Persist any existing starting values.
    for (auto& [sequence, value] : persisted_values_) {
      if (params.ShouldPersist(sequence)) {
        temp[sequence] = value;
      }
    }

    // Persist any current values, replacing starting values.
    for (auto& [sequence, value] : current_values_) {
      if (params.ShouldPersist(sequence)) {
        temp[sequence] = value;
      }
    }

    current_values_.clear();
    persisted_values_ = std::move(temp);
  }

  ValueLookup persisted_values_;
  ValueLookup current_values_;
  BrowserAnimationMotion current_motion_;
  MotionInfo current_motion_info_;
  std::optional<Animator> animator_;
  std::optional<Logger> logger_;

  // The list of listeners. Note that this is destroyed before the animation,
  // so no messages are sent on teardown. This severs any potentially unsafe
  // dependency cycles and prevents re-entrancy during destruction.
  base::RepeatingCallbackList<void(const BrowserAnimationController*,
                                   BrowserAnimationUpdate)>
      callbacks_;

  const raw_ref<const BrowserAnimationController> controller_;
  const BrowserAnimationGroup group_;
};

BrowserAnimationController::BrowserAnimationController(
    BrowserWindowInterface& browser)
    : scoped_user_data_(browser.GetUnownedUserDataHost(), *this) {}

BrowserAnimationController::~BrowserAnimationController() = default;

// static
BrowserAnimationController* BrowserAnimationController::From(
    BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

// static
const BrowserAnimationController* BrowserAnimationController::From(
    const BrowserWindowInterface* browser) {
  return Get(browser->GetUnownedUserDataHost());
}

bool BrowserAnimationController::IsAnimating(
    BrowserAnimationGroup group) const {
  return static_cast<bool>(GetCurrentMotion(group));
}

BrowserAnimationMotion BrowserAnimationController::GetCurrentMotion(
    BrowserAnimationGroup group) const {
  return GetGroupData(group).current_motion();
}

base::TimeDelta BrowserAnimationController::GetMotionDuration(
    BrowserAnimationGroup group) const {
  return GetGroupData(group).GetMotionDuration();
}

void BrowserAnimationController::Start(
    BrowserAnimationGroup group,
    BrowserAnimationMotion motion,
    std::optional<std::string_view> group_histogram_override,
    std::optional<std::string_view> motion_histogram_override) {
  auto motion_info = GetMotionInfo(group, motion);
  CHECK(motion_info.has_value());
  if (group_histogram_override) {
    motion_info->histogram_prefix.group_name = *group_histogram_override;
  }
  if (motion_histogram_override) {
    motion_info->histogram_prefix.motion_name = *motion_histogram_override;
  }
  GetGroupData(group).Start(motion, std::move(*motion_info));
}

void BrowserAnimationController::Reset(BrowserAnimationGroup group,
                                       BrowserAnimationMotion motion) {
  auto& group_data = GetGroupData(group);
  if (!motion) {
    motion = group_data.current_motion();
    if (!motion) {
      return;
    }
  }
  auto motion_info = GetMotionInfo(group, motion);
  group_data.Reset(motion, std::move(*motion_info));
}

bool BrowserAnimationController::Clear(BrowserAnimationGroup group) {
  return GetGroupData(group).Cancel(/*clear=*/true);
}

std::optional<double> BrowserAnimationController::GetCurrentValue(
    BrowserAnimationGroup group,
    BrowserAnimationSequence sequence) const {
  const auto& group_data = GetGroupData(group);
  auto result = group_data.GetCurrentValue(sequence);
  if (!result) {
    const auto params = GetSequenceParams(group, sequence);
    if (params &&
        (params->persist_between_animations || group_data.current_motion())) {
      result = params->default_value;
    }
  }
  return result;
}

base::CallbackListSubscription BrowserAnimationController::Subscribe(
    BrowserAnimationGroup group,
    BrowserAnimationCallback callback) {
  return GetGroupData(group).Subscribe(std::move(callback));
}

std::optional<BrowserAnimationController::MotionInfo>
BrowserAnimationController::GetMotionInfo(BrowserAnimationGroup group,
                                          BrowserAnimationMotion motion) const {
  MotionInfo result;
  for (const auto& provider : base::Reversed(providers_)) {
    auto spec = provider->GetMotionSpecification(group, motion);
    if (spec) {
      result.motion = std::move(*spec);
      result.duration = result.motion.GetDuration();
      result.histogram_prefix = provider->GetHistogramPrefix(group, motion);
      return result;
    }
  }
  return std::nullopt;
}

std::optional<internal::BrowserAnimationMotionSpecification>
BrowserAnimationController::GetMotionSpecificationForTesting(
    BrowserAnimationGroup group,
    BrowserAnimationMotion motion) const {
  const auto result = GetMotionInfo(group, motion);
  return result ? std::make_optional(result->motion) : std::nullopt;
}

std::unique_ptr<BrowserAnimationProvider>
BrowserAnimationController::RemoveProviderForTesting(
    BrowserAnimationProvider* provider) {
  const auto it = std::ranges::find_if(
      providers_,
      [provider](const auto& value) { return value.get() == provider; });
  if (it == providers_.end()) {
    return nullptr;
  }
  std::unique_ptr<BrowserAnimationProvider> old = std::move(*it);
  providers_.erase(it);
  return old;
}

void BrowserAnimationController::SetAnimationContainerForTesting(
    BrowserAnimationGroup group,
    gfx::AnimationContainer* container) {
  return GetGroupData(group).SetAnimationContainerForTesting(  // IN-TEST
      container);
}

BrowserAnimationController::GroupData& BrowserAnimationController::GetGroupData(
    BrowserAnimationGroup group) {
  return const_cast<BrowserAnimationController::GroupData&>(
      const_cast<const BrowserAnimationController*>(this)->GetGroupData(group));
}

const BrowserAnimationController::GroupData&
BrowserAnimationController::GetGroupData(BrowserAnimationGroup group) const {
  auto it = execution_data_.find(group);
  if (it == execution_data_.end()) {
    // When we "discover" a new group, allocate data for it. Group data is
    // created on demand. The number of groups is bounded at compile time, and
    // the data is freed on window close.
    it = execution_data_
             .emplace(group, std::make_unique<GroupData>(*this, group))
             .first;
  }
  return *it->second.get();
}

internal::BrowserAnimationSequenceParamsLookup
BrowserAnimationController::GetAllSequenceParams(
    BrowserAnimationGroup group) const {
  for (const auto& provider : base::Reversed(providers_)) {
    auto result = provider->GetAllSequenceParams(group);
    if (!result.empty()) {
      return result;
    }
  }
  return {};
}

std::optional<internal::BrowserAnimationSequenceParams>
BrowserAnimationController::GetSequenceParams(
    BrowserAnimationGroup group,
    BrowserAnimationSequence sequence) const {
  // Find the first provider which specifies the sequence.
  for (const auto& provider : base::Reversed(providers_)) {
    if (const auto params = provider->GetSequenceParams(group, sequence)) {
      return params;
    }
  }

  return std::nullopt;
}
