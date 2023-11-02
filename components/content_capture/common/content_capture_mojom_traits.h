// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_MOJOM_TRAITS_H_
#define COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_MOJOM_TRAITS_H_

#include <vector>

#include "components/content_capture/common/content_capture_data.h"
#include "components/content_capture/common/content_capture_data.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/rect_f.h"

namespace mojo {

template <>
class StructTraits<content_capture::mojom::ContentCaptureDataDataView,
                   content_capture::ContentCaptureData> {
 public:
  static int64_t id(const content_capture::ContentCaptureData& r) {
    return r.id;
  }
  static const std::u16string& value(
      const content_capture::ContentCaptureData& r) {
    return r.value;
  }
  static const gfx::Rect& bounds(const content_capture::ContentCaptureData& r) {
    return r.bounds;
  }
  static const std::vector<content_capture::ContentCaptureData>& children(
      const content_capture::ContentCaptureData& r) {
    return r.children;
  }

  static bool Read(content_capture::mojom::ContentCaptureDataDataView data,
                   content_capture::ContentCaptureData* out_data);
};

}  // namespace mojo

#endif  // COMPONENTS_CONTENT_CAPTURE_COMMON_CONTENT_CAPTURE_MOJOM_TRAITS_H_
