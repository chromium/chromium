// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_PROTO_VALIDATOR_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_PROTO_VALIDATOR_H_

#include "components/paint_preview/common/proto/paint_preview.pb.h"

namespace paint_preview {

// Verifies that all the expected fields of `paint_preview` are present. If this
// returns true, downstream callers can treat all fields in the proto that are
// stated to be required in paint_preview.proto to be present.
bool PaintPreviewProtoValid(const PaintPreviewProto& paint_preview);

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_PROTO_VALIDATOR_H_
