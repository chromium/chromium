// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/android/input_receiver_data.h"

#include <utility>

namespace input {

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
      viz_input_token_(std::move(viz_input_token)) {}

InputReceiverData::~InputReceiverData() = default;

void InputReceiverData::OnDestroyedCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id) {
  if (root_frame_sink_id() != frame_sink_id) {
    return;
  }
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
