// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_PROTOBUFS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_PROTOBUFS_H_

#include "components/autofill/core/common/logging/log_buffer.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace autofill {

// Serialize a repeated field in a protobuf. This function is not part of
// log_buffer.h because that would create a dependency of the renderer process
// to protobufs.
template <typename T>
LogBuffer& operator<<(LogBuffer& buf,
                      const ::google::protobuf::RepeatedField<T>& values) {
  buf << "[";
  for (int i = 0; i < values.size(); ++i) {
    if (i)
      buf << ", ";
    buf << values.Get(i);
  }
  buf << "]";
  return buf;
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_LOGGING_LOG_PROTOBUFS_H_
