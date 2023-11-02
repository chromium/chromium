// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_FRAME_H_
#define COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_FRAME_H_

#include "components/content_capture/common/content_capture_data.h"

namespace content_capture {

// This struct defines a captured frame in the browser side, its children are
// the captured content if available. It is also used to represent the frame
// hierarchy in ContentCaptureSession.
struct ContentCaptureFrame {
  ContentCaptureFrame();
  ContentCaptureFrame(const ContentCaptureFrame& data);
  // Construct ContentCaptureFrame from the |data| whose root is a frame.
  explicit ContentCaptureFrame(const ContentCaptureData& data);
  ~ContentCaptureFrame();

  // The id of the frame this will be 0 until ContentCaptureReceiver assigns a
  // unique ID.
  int64_t id = 0;
  // The url of frame.
  std::u16string url;
  // The bounds of the frame.
  gfx::Rect bounds;
  // The content of this frame, might not always available.
  std::vector<ContentCaptureData> children;
  // The title of a page.
  std::u16string title;
  // The json that represents std::vector<blink::mojom::FaviconURLPtr>.
  // The below example has two favicons, the second favicon files has two
  // images.
  // [
  //    {
  //      "sizes" :
  //      [
  //        {
  //          "height" : 192,
  //          "width" : 192
  //        }
  //      ],
  //      "type" : "favicon",
  //      "url" : "https://www.abc.com/appicon-192.png"
  //    },
  //    {
  //      "sizes" :
  //      [
  //        {
  //          "height" : 144,
  //          "width" : 144
  //        }
  //       {
  //          "height" : 192,
  //          "width" : 192
  //        }
  //      ],
  //      "type" : "touch icon",
  //      "url" : "https://www.abc.com/appicon.png"
  //    }
  //  ]
  // The 'type' could be 'favicon', 'touch icon' and 'touch precomposed icon'.
  std::string favicon;

  bool operator==(const ContentCaptureFrame& other) const;

  bool operator!=(const ContentCaptureFrame& other) const {
    return !(*this == other);
  }
};

// This defines a session, is a list of frames from current frame to root.
// This represents the frame hierarchy, starting from the current frame to the
// root frame, in the upward order. Note that ContentCaptureFrame here can only
// have URL as value, and no ContentCaptureFrame has children in it.
using ContentCaptureSession = std::vector<ContentCaptureFrame>;

}  // namespace content_capture

#endif  // COMPONENTS_CONTENT_CAPTURE_BROWSER_CONTENT_CAPTURE_FRAME_H_
