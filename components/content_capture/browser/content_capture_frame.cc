// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/browser/content_capture_frame.h"

namespace content_capture {

ContentCaptureFrame::ContentCaptureFrame() = default;

ContentCaptureFrame::ContentCaptureFrame(const ContentCaptureFrame& data) =
    default;

ContentCaptureFrame::ContentCaptureFrame(const ContentCaptureData& data) {
  url = data.value;
  bounds = data.bounds;
  children = data.children;
}

ContentCaptureFrame::~ContentCaptureFrame() = default;

bool ContentCaptureFrame::operator==(const ContentCaptureFrame& other) const {
  return id == other.id && url == other.url && bounds == other.bounds &&
         children == other.children && title == other.title;
}

}  // namespace content_capture
