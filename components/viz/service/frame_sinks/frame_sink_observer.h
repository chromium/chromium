// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_OBSERVER_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_OBSERVER_H_

namespace viz {

class FrameSinkId;
struct BeginFrameArgs;

class FrameSinkObserver {
 public:
  virtual ~FrameSinkObserver() = default;

  // Called when FrameSinkId is registered
  virtual void OnRegisteredFrameSinkId(const FrameSinkId& frame_sink_id) {}

  // Called when FrameSinkId is being invalidated
  virtual void OnInvalidatedFrameSinkId(const FrameSinkId& frame_sink_id) {}

  // Called when CompositorFrameSink is created
  virtual void OnCreatedCompositorFrameSink(const FrameSinkId& frame_sink_id,
                                            bool is_root) {}

  // Called when CompositorFrameSink is about to be destroyed
  virtual void OnDestroyedCompositorFrameSink(
      const FrameSinkId& frame_sink_id) {}

  // Called when |parent_frame_sink_id| becomes a parent of
  // |child_frame_sink_id|
  virtual void OnRegisteredFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) {}

  // Called when |parent_frame_sink_id| stops being a parent of
  // |child_frame_sink_id|
  virtual void OnUnregisteredFrameSinkHierarchy(
      const FrameSinkId& parent_frame_sink_id,
      const FrameSinkId& child_frame_sink_id) {}

  // Called when a sink has started a frame.
  virtual void OnFrameSinkDidBeginFrame(const FrameSinkId& frame_sink_id,
                                        const BeginFrameArgs& args) {}

  // Called when a sink has finished processing a frame.
  virtual void OnFrameSinkDidFinishFrame(const FrameSinkId& frame_sink_id,
                                         const BeginFrameArgs& args) {}

  // Called when capturing is started for `frame_sink_id`.
  virtual void OnCaptureStarted(const FrameSinkId& frame_sink_id) {}
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_FRAME_SINK_OBSERVER_H_
