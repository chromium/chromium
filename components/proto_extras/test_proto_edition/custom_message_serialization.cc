// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "components/proto_extras/test_proto_edition/test_proto_edition.pb.h"

namespace proto_extras {

base::Value ToValue(const CustomSerializedMessage& message) {
  return base::Value("custom serialized");
}

}  // namespace proto_extras
