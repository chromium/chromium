// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PROTO_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PROTO_UTIL_H_

#include "components/optimization_guide/proto/common_types.pb.h"

namespace optimization_guide::proto {
class Any;
class AXTreeUpdate;
}  // namespace optimization_guide::proto

namespace ui {
struct AXTreeUpdate;
}  // namespace ui

namespace optimization_guide {

// Constructs an Any proto containing the given message.
proto::Any AnyWrapProto(const google::protobuf::MessageLite& m);

// Populate the AXTreeUpdate proto structure from the ui structure.
void PopulateAXTreeUpdateProto(
    const ui::AXTreeUpdate& source,
    optimization_guide::proto::AXTreeUpdate* destination);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_OPTIMIZATION_GUIDE_PROTO_UTIL_H_
