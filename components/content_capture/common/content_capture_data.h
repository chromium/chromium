// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_DATA_H_
#define COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_DATA_H_

#include <vector>

#include "base/strings/string16.h"
#include "ui/gfx/geometry/rect_f.h"

namespace content_capture {

// This struct defines the on-screen text content and its bounds in a frame,
// the text is captured in renderer and sent to browser; the root of
// this tree is frame, the child could be the scrollable area or the text
// content, scrollable area can have scrollable area or text as child. Text
// cannot have any child.
struct ContentCaptureData {
  ContentCaptureData();
  ContentCaptureData(const ContentCaptureData& data);
  ~ContentCaptureData();

  // The id of the frame or the content,
  // For frame, this will be 0 until ContentCaptureReceiver assigns a unique ID.
  int64_t id = 0;
  // The url of frame or the text of the content.
  // For frame, this is the URL of the frame.
  // For scrollable area, this is not used.
  // For text, this is the text value.
  base::string16 value;
  // The bounds of the frame or the content.
  gfx::Rect bounds;
  // The children content, only available for frame or scrollable area.
  std::vector<ContentCaptureData> children;

  bool operator==(const ContentCaptureData& other) const;

  bool operator!=(const ContentCaptureData& other) const {
    return !(*this == other);
  }
};

// This defines a session, is a list of frames from current frame to root.
// This represents the frame hierarchy, starting from the current frame to the
// root frame, in the upward order. Note that ContentCaptureData here can only
// have URL as value, and no ContentCaptureData has children in it.
using ContentCaptureSession = std::vector<ContentCaptureData>;

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_DATA_H_
