// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/manta/anchovy/anchovy_requests.h"
#include "components/manta/manta_service_callbacks.h"
#include "components/manta/manta_status.h"
#include "components/manta/proto/manta.pb.h"

#ifndef COMPONENTS_MANTA_ANCHOVY_ANCHOVY_PROTO_HELPER_H_
#define COMPONENTS_MANTA_ANCHOVY_ANCHOVY_PROTO_HELPER_H_

namespace manta::anchovy {

class AnchovyProtoHelper {
 public:
  static proto::Request ComposeRequest(
      const anchovy::ImageDescriptionRequest& request);

  static void HandleImageDescriptionResponse(
      MantaGenericCallback callback,
      std::unique_ptr<proto::Response> manta_response,
      MantaStatus manta_status);
};

}  // namespace manta::anchovy

#endif  // COMPONENTS_MANTA_ANCHOVY_ANCHOVY_PROTO_HELPER_H_
