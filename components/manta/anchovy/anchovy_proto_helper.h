// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_ANCHOVY_ANCHOVY_PROTO_HELPER_H_
#define COMPONENTS_MANTA_ANCHOVY_ANCHOVY_PROTO_HELPER_H_

#include <memory>

#include "base/component_export.h"
#include "components/manta/anchovy/anchovy_requests.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"

namespace manta::anchovy {

class COMPONENT_EXPORT(MANTA) AnchovyProtoHelper {
 public:
  static proto::Request ComposeRequest(
      const anchovy::ImageDescriptionRequest& request);
  // Serializes `request` into a Manta `proto::Request`. Every entry in
  // `request.images` must have non-empty `image_bytes` and a unique, non-empty
  // UTF-8 `tag`.
  static proto::Request ComposeRequest(
      const ImageDescriptionBatchRequest& request);

  static void HandleImageDescriptionResponse(
      MantaGenericCallback callback,
      std::unique_ptr<proto::Response> manta_response,
      MantaStatus manta_status);
  // Parses `manta_response` and runs `callback` with a dictionary containing
  // `kResultsByTagKey` when `manta_status.status_code == MantaStatusCode::kOk`,
  // or an empty dictionary otherwise.
  static void HandleImageDescriptionsResponse(
      MantaGenericCallback callback,
      std::unique_ptr<proto::Response> manta_response,
      MantaStatus manta_status);
};

}  // namespace manta::anchovy

#endif  // COMPONENTS_MANTA_ANCHOVY_ANCHOVY_PROTO_HELPER_H_
