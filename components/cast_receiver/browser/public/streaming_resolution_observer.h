// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_STREAMING_RESOLUTION_OBSERVER_H_
#define COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_STREAMING_RESOLUTION_OBSERVER_H_

#include "base/observer_list_types.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace media {
struct VideoTransformation;
}  // namespace media

namespace cast_receiver {

// Observer to be informed when the resolution of an ongoing Cast mirroring or
// remoting stream (as supported by the cast_streaming component) is changed.
// TODO(crbug.com/1358690): Remove this class.
class StreamingResolutionObserver : public base::CheckedObserver {
 public:
  ~StreamingResolutionObserver() override = default;

  // Called when the running streaming application changes the resolution of
  // its generated video frames.
  virtual void OnStreamingResolutionChanged(
      const gfx::Rect& size,
      const media::VideoTransformation& transformation) = 0;
};

}  // namespace cast_receiver

#endif  // COMPONENTS_CAST_RECEIVER_BROWSER_PUBLIC_STREAMING_RESOLUTION_OBSERVER_H_
