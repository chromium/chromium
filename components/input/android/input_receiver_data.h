// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_ANDROID_INPUT_RECEIVER_DATA_H_
#define COMPONENTS_INPUT_ANDROID_INPUT_RECEIVER_DATA_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "components/input/android/android_input_callback.h"
#include "components/input/android/scoped_input_receiver.h"
#include "components/input/android/scoped_input_receiver_callbacks.h"
#include "components/input/android/scoped_input_transfer_token.h"
#include "ui/gfx/android/android_surface_control_compat.h"

namespace input {

// Store all the obects that are required throughout input receiver lifecycle
class COMPONENT_EXPORT(INPUT) InputReceiverData {
 public:
  InputReceiverData(
      scoped_refptr<gfx::SurfaceControl::Surface> parent_input_sc,
      scoped_refptr<gfx::SurfaceControl::Surface> input_sc,
      ScopedInputTransferToken browser_input_token,
      std::unique_ptr<AndroidInputCallback> android_input_callback,
      ScopedInputReceiverCallbacks callbacks,
      ScopedInputReceiver receiver,
      ScopedInputTransferToken viz_input_token);

  ~InputReceiverData();

  void OnDestroyedCompositorFrameSink(const viz::FrameSinkId& frame_sink_id);

  const viz::FrameSinkId& root_frame_sink_id() {
    return android_input_callback_->root_frame_sink_id();
  }

  // Attaches(reparents) the input surface control to a surface control linked
  // to surface hierarchy(`parent_input_sc`) of root compositor. The "detach"
  // happens when we receive OnDestroyedCompositorFrameSink notification for the
  // attached root compositor frame sink.
  void AttachToFrameSink(
      const viz::FrameSinkId& root_frame_sink_id,
      scoped_refptr<gfx::SurfaceControl::Surface> parent_input_sc);

  const ScopedInputTransferToken& viz_input_token() const {
    return viz_input_token_;
  }
  const ScopedInputTransferToken& browser_input_token() const {
    return browser_input_token_;
  }

 private:
  void DetachInputSurface();

  scoped_refptr<gfx::SurfaceControl::Surface> parent_input_sc_;
  scoped_refptr<gfx::SurfaceControl::Surface> input_sc_;
  ScopedInputTransferToken browser_input_token_;
  std::unique_ptr<AndroidInputCallback> android_input_callback_;
  ScopedInputReceiverCallbacks callbacks_;
  ScopedInputReceiver receiver_;
  ScopedInputTransferToken viz_input_token_;
  bool pending_destruction_ = false;
  base::WeakPtrFactory<InputReceiverData> weak_ptr_factory_{this};
};

}  // namespace input

#endif  // COMPONENTS_INPUT_ANDROID_INPUT_RECEIVER_DATA_H_
