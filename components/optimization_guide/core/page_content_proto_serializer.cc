// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/page_content_proto_serializer.h"

#include "base/unguessable_token.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "url/origin.h"

namespace optimization_guide {

void SecurityOriginSerializer::Serialize(
    const url::Origin& origin,
    optimization_guide::proto::SecurityOrigin* proto_origin) {
  proto_origin->set_opaque(origin.opaque());

  if (origin.opaque()) {
    proto_origin->set_value(origin.GetNonceForSerialization()->ToString());
  } else {
    proto_origin->set_value(origin.Serialize());
  }
}

}  // namespace optimization_guide
