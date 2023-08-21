// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ALLOCATION_RECORDER_INTERNAL_INTERNAL_H_
#define COMPONENTS_ALLOCATION_RECORDER_INTERNAL_INTERNAL_H_

#include "third_party/crashpad/crashpad/client/annotation.h"

#include <string>  // for std::char_traits

namespace allocation_recorder::internal {

// The name of the annotation that is used to pass data from crash client to
// crash handler.
constexpr char kAnnotationName[] = "allocation-recorder-crash-info";
static_assert(std::char_traits<char>::length(kAnnotationName) <
              crashpad::Annotation::kNameMaxLength);

// The type of the annotation. To avoid conflicts with other user defined types
// we use a more complex number than 1. Note that we store the address of the
// recorder in the annotation.
constexpr crashpad::Annotation::Type kAnnotationType =
    crashpad::Annotation::UserDefinedType(0xa10);

// The stream data type passed to MinidumpUserExtensionStreamDataSource, for
// details please see minidump_user_extension_stream_data_source.h in
// third_party/crashpad/crashpad/minidump/
constexpr uint32_t kStreamDataType = 0x3A5F9C7B;

}  // namespace allocation_recorder::internal
#endif  // COMPONENTS_ALLOCATION_RECORDER_INTERNAL_INTERNAL_H_
