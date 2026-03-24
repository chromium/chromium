// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/animation/browser_animation_controller.h"

#include <algorithm>
#include <concepts>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "base/callback_list.h"
#include "base/check_is_test.h"
#include "base/containers/adapters.h"
#include "base/containers/map_util.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chrome/browser/ui/animation/browser_animation_provider.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/animation/animation_delegate_views.h"

namespace {

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

  void SetAnimationContainer(gfx::AnimationContainer* container) {
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

template <typename T>
double GetSequenceValueT(
    const internal::BrowserAnimationSequenceSpecification& sequence,
    T absolute_time) {
  double prev_value = sequence.keyframes.front().value;
  T prev_frame_time = T();
  for (const auto& frame : sequence.keyframes) {
    T frame_time;
    if constexpr (std::same_as<T, double>) {
      frame_time = frame.time.percent().value_or(0.0);
    } else {
      frame_time = frame.time.absolute_time().value_or(base::Milliseconds(0));
    }
    if (absolute_time == frame_time) {
      return frame.value;
    }
    if (absolute_time < frame_time) {
      const double percent =
          (absolute_time - prev_frame_time) / (frame_time - prev_frame_time);
      const double tween_value =
          gfx::Tween::CalculateValue(frame.tween_type, percent);
      return prev_value + (frame.value - prev_value) * tween_value;
    }
    prev_value = frame.value;
    prev_frame_time = frame_time;
  }
  return prev_value;
}

// Calculates the value for sequence described by `spec` at time `time`.
double GetSequenceValue(
    const internal::BrowserAnimationSequenceSpecification& sequence,
    base::TimeDelta absolute_time,
    double scaled_percent) {
  if (sequence.keyframes.back().time.percent()) {
    return GetSequenceValueT(sequence, scaled_percent);
  } else if (sequence.keyframes.back().time.absolute_time()) {
    return GetSequenceValueT(sequence, absolute_time);
  } else {
    return sequence.keyframes.front().value;
  }
}

}  // namespace

DEFINE_USER_DATA(BrowserAnimationController);

// Tracks the current motion and progress for a particular animation group/
class BrowserAnimationController::GroupData : public gfx::AnimationDelegate {
 public:
  GroupData(const BrowserAnimationController& controller,
            BrowserAnimationGroup group)
      : controller_(controller), group_(group) {}

  ~GroupData() override = default;

  void Start(BrowserAnimationMotion motion,
             internal::BrowserAnimationMotionSpecification motion_spec) {
    // TODO(dfried): figure out how to redirect animations.
    Cancel();
    MaybeCreateAnimator();
    current_motion_ = motion;
    current_motion_spec_ = std::move(motion_spec);
    for (const auto& [sequence, sequence_spec] :
         current_motion_spec_.sequences) {
      current_values_.emplace(sequence, sequence_spec.keyframes.front().value);
    }
    current_duration_ = current_motion_spec_.GetDuration();
    animator_->Start(current_duration_);
    Notify(BrowserAnimationUpdate::kStarted);
  }

  bool Cancel() {
    if (!current_motion_) {
      return false;
    }
    current_motion_ = BrowserAnimationMotion();
    current_values_.clear();
    animator_->Stop();
    Notify(BrowserAnimationUpdate::kCanceled);
    return true;
  }

  std::optional<double> GetCurrentValue(
      BrowserAnimationSequence sequence) const {
    auto* value = base::FindOrNull(current_values_, sequence);
    return value ? std::make_optional(*value) : std::nullopt;
  }

  base::TimeDelta GetMotionDuration() const {
    if (!current_motion_) {
      return base::Seconds(0);
    }
    return current_motion_spec_.GetDuration();
  }

  bool has_animator() const { return animator_ != nullptr; }

  BrowserAnimationMotion current_motion() const { return current_motion_; }

  base::CallbackListSubscription Subscribe(BrowserAnimationCallback callback) {
    return callbacks_.Add(std::move(callback));
  }

  void SetAnimationContainer(gfx::AnimationContainer* container) {
    MaybeCreateAnimator();
    animator_->SetAnimationContainer(container);
  }

 private:
  void MaybeCreateAnimator() {
    if (!animator_) {
      views::View* const view = controller_->browser_view_;
      if (!view) {
        CHECK_IS_TEST()
            << "Can only attempt to start animations without a view in tests.";
      }
      animator_ = std::make_unique<Animator>(*this, view);
    }
  }

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override {
    UpdateValues(current_duration_, 1.0);
    Notify(BrowserAnimationUpdate::kEnded);
    current_motion_ = BrowserAnimationMotion();
    current_values_.clear();
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    const double unscaled_percent = animation->GetCurrentValue();
    const double scaled_percent = gfx::Tween::CalculateValue(
        current_motion_spec_.global_tween, unscaled_percent);
    const base::TimeDelta absolute_time = current_duration_ * unscaled_percent;
    UpdateValues(absolute_time, scaled_percent);
    Notify(BrowserAnimationUpdate::kProgressed);
  }

  void UpdateValues(base::TimeDelta absolute_time, double scaled_percent) {
    CHECK(current_motion_);
    for (const auto& [sequence, sequence_spec] :
         current_motion_spec_.sequences) {
      current_values_[sequence] =
          GetSequenceValue(sequence_spec, absolute_time, scaled_percent);
    }
  }

  void Notify(BrowserAnimationUpdate status) {
    callbacks_.Notify(&controller_.get(), status);
  }

  std::map<BrowserAnimationSequence, double> current_values_;
  BrowserAnimationMotion current_motion_;
  internal::BrowserAnimationMotionSpecification current_motion_spec_;
  base::TimeDelta current_duration_;
  std::unique_ptr<Animator> animator_;

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

void BrowserAnimationController::Start(BrowserAnimationGroup group,
                                       BrowserAnimationMotion motion) {
  auto motion_spec = GetMotionSpecification(group, motion);
  CHECK(motion_spec.has_value());
  GetGroupData(group).Start(motion, std::move(motion_spec.value()));
}

bool BrowserAnimationController::Cancel(BrowserAnimationGroup group) {
  return GetGroupData(group).Cancel();
}

std::optional<double> BrowserAnimationController::GetCurrentValue(
    BrowserAnimationGroup group,
    BrowserAnimationSequence sequence) const {
  return GetGroupData(group).GetCurrentValue(sequence);
}

base::CallbackListSubscription BrowserAnimationController::Subscribe(
    BrowserAnimationGroup group,
    BrowserAnimationCallback callback) {
  return GetGroupData(group).Subscribe(std::move(callback));
}

std::optional<internal::BrowserAnimationMotionSpecification>
BrowserAnimationController::GetMotionSpecification(
    BrowserAnimationGroup group,
    BrowserAnimationMotion motion) const {
  for (const auto& provider : base::Reversed(providers_)) {
    auto result = provider->GetMotionSpecification(group, motion);
    if (result) {
      return result;
    }
  }
  return std::nullopt;
}

std::optional<internal::BrowserAnimationMotionSpecification>
BrowserAnimationController::GetMotionSpecificationForTesting(
    BrowserAnimationGroup group,
    BrowserAnimationMotion motion) const {
  return GetMotionSpecification(group, motion);
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
  return GetGroupData(group).SetAnimationContainer(container);
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
