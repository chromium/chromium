// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_CAPTURE_RESULT_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_CAPTURE_RESULT_H_

#include "base/containers/flat_map.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

// A subset of PaintPreviewCaptureParams that will be filled in by
// PaintPreviewClient. This type mainly exists to aggregate related parameters.
struct RecordingParams {
  explicit RecordingParams(const base::UnguessableToken& document_guid);

  // The document GUID for this capture.
  const base::UnguessableToken document_guid;

  // The rect to which to clip the capture to.
  gfx::Rect clip_rect;

  // Whether the capture is for the main frame or an OOP subframe.
  bool is_main_frame;

  // For the following params, The values set here for the first frame apply to
  // all subframes that are captured.

  // Whether to record links.
  bool capture_links;

  // The maximum capture size allowed for the SkPicture captured. A size of 0 is
  // unlimited.
  // TODO(crbug.com/40126774): Ideally, this would cap the total size rather
  // than being a per SkPicture limit. However, that is non-trivial due to the
  // async ordering of captures from different frames making it hard to keep
  // track of available headroom at the time of each capture triggering.
  size_t max_capture_size;

  // Limit on the maximum size of a decoded image that can be serialized.
  // Any images with a decoded size exceeding this value will be discarded.
  // This can be used to reduce the chance of an OOM during serialization and
  // later during playback.
  uint64_t max_decoded_image_size_bytes{std::numeric_limits<uint64_t>::max()};

  // This flag will skip GPU accelerated content where applicable when
  // capturing. This reduces hangs, capture time and may also reduce OOM
  // crashes, but results in a lower fideltiy capture (i.e. the contents
  // captured may not accurately reflect the content visible to the user at
  // time of capture). See PaintPreviewBaseService::CaptureParams for a
  // description of the effects of this flag.
  bool skip_accelerated_content{false};
};

// The result of a capture of a WebContents, which may contain recordings of
// multiple subframes.
struct CaptureResult {
 public:
  explicit CaptureResult(RecordingPersistence persistence);
  ~CaptureResult();

  CaptureResult(CaptureResult&&);
  CaptureResult& operator=(CaptureResult&&);

  // Will match the |persistence| in the original capture request.
  RecordingPersistence persistence;

  PaintPreviewProto proto = {};

  // Maps frame embedding tokens to buffers containing the serialized
  // recordings. See |PaintPreviewCaptureResponse::skp| for information on how
  // to intepret these buffers. Empty if |RecordingPersistence::FileSystem|.
  base::flat_map<base::UnguessableToken, mojo_base::BigBuffer> serialized_skps =
      {};

  // Indicates that at least one subframe finished successfully.
  bool capture_success = false;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_CAPTURE_RESULT_H_
