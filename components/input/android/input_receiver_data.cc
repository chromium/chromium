// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/android/input_receiver_data.h"

#include <utility>

#include "base/android/android_info.h"

namespace input {

namespace {

// Time limit at which InputReceiver is destroyed without waiting
// for complete of touch sequence to arrive from Viz.
const base::TimeDelta kTimeToWaitForLastEvent = base::Seconds(2);
// Time limit used to say we are probably not going to get any
// more events from system, and if an input receiver destruction
// timer has fired let's indeed destroy it.
const base::TimeDelta kInactiveSequenceThreshold = base::Seconds(1);
}  // namespace

InputReceiverData::InputReceiverData(
    scoped_refptr<gfx::SurfaceControl::Surface> parent_input_sc,
    scoped_refptr<gfx::SurfaceControl::Surface> input_sc,
    ScopedInputTransferToken browser_input_token,
    std::unique_ptr<AndroidInputCallback> android_input_callback,
    ScopedInputReceiverCallbacks callbacks,
    ScopedInputReceiver receiver,
    ScopedInputTransferToken viz_input_token)
    : parent_input_sc_(parent_input_sc),
      input_sc_(input_sc),
      browser_input_token_(std::move(browser_input_token)),
      android_input_callback_(std::move(android_input_callback)),
      callbacks_(std::move(callbacks)),
      receiver_(std::move(receiver)),
      viz_input_token_(std::move(viz_input_token)) {
  android_input_callback_->AddObserver(this);
}

InputReceiverData::~InputReceiverData() {
  android_input_callback_->RemoveObserver(this);
}

void InputReceiverData::OnMotionEvent(
    const base::android::ScopedInputEvent& input_event) {
  const int action = AMotionEvent_getAction(input_event.a_input_event()) &
                     AMOTION_EVENT_ACTION_MASK;
  last_motion_event_action_ = action;
  last_motion_event_ts_ = base::TimeTicks::Now();
}

void InputReceiverData::TryDestroySelf(
    std::unique_ptr<InputReceiverData> receiver_data) {
  CHECK(receiver_data);
  const base::TimeDelta time_since_last_motion_event =
      base::TimeTicks::Now() - last_motion_event_ts_;
  if (last_motion_event_action_ == AMOTION_EVENT_ACTION_CANCEL ||
      last_motion_event_action_ == AMOTION_EVENT_ACTION_UP ||
      time_since_last_motion_event > kInactiveSequenceThreshold) {
    // InputReceiverData gets destroyed here.
    return;
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InputReceiverData::TryDestroySelf,
                     base::Unretained(receiver_data.get()),
                     std::move(receiver_data)),
      kTimeToWaitForLastEvent);
}

void InputReceiverData::OnDestroyedCompositorFrameSink(
    std::unique_ptr<InputReceiverData> receiver) {
  if (base::android::android_info::sdk_int() >=
      base::android::android_info::SdkVersion::SDK_VERSION_BAKLAVA) {
    // For Android 16+ where input receiver could be destroyed, `receiver` would
    // have non-nullptr in it, which should be cleanup eventually after seeing
    // the full touch sequence or after giving up upon new input events to come.
    CHECK_EQ(receiver.get(), this);
    const base::TimeDelta time_since_last_motion_event =
        base::TimeTicks::Now() - last_motion_event_ts_;
    if (time_since_last_motion_event > kInactiveSequenceThreshold ||
        (last_motion_event_action_ == AMOTION_EVENT_ACTION_CANCEL ||
         last_motion_event_action_ == AMOTION_EVENT_ACTION_UP)) {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(
                         [](std::unique_ptr<InputReceiverData>) {
                           // InputReceiverData gets destroyed here.
                         },
                         std::move(receiver)));
    } else {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&InputReceiverData::TryDestroySelf,
                         base::Unretained(receiver.get()), std::move(receiver)),
          kTimeToWaitForLastEvent);
    }
    return;
  }
  CHECK(!receiver);
  pending_destruction_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&InputReceiverData::DetachInputSurface,
                                weak_ptr_factory_.GetWeakPtr()));
}

void InputReceiverData::AttachToFrameSink(
    const viz::FrameSinkId& root_frame_sink_id,
    scoped_refptr<gfx::SurfaceControl::Surface> parent_input_sc) {
  DCHECK(!android_input_callback_->root_frame_sink_id().is_valid());

  // In case a new root compositor frame sink gets created before
  // DetachInputSurface had a chance to run. In this case DetachInputSurface
  // shouldn't do anything in the pending detach task.
  pending_destruction_ = false;

  parent_input_sc_ = parent_input_sc;

  gfx::SurfaceControl::Transaction transaction;
  transaction.SetParent(*input_sc_, parent_input_sc_.get());
  transaction.Apply();

  android_input_callback_->set_root_frame_sink_id(root_frame_sink_id);
}

void InputReceiverData::DetachInputSurface() {
  if (!pending_destruction_) {
    return;
  }

  pending_destruction_ = false;

  gfx::SurfaceControl::Transaction transaction;
  transaction.SetParent(*input_sc_, nullptr);
  transaction.Apply();

  parent_input_sc_.reset();

  android_input_callback_->set_root_frame_sink_id(viz::FrameSinkId());
}

}  // namespace input
