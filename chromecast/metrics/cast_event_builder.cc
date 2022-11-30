// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/cast_event_builder.h"

#include "third_party/metrics_proto/cast_logs.pb.h"

namespace chromecast {

// static
void CastEventBuilder::SetLaunchFromProto(
    ::metrics::CastLogsProto_CastEventProto* out,
    LaunchFrom launch_from) {
  switch (launch_from) {
    case FROM_UNKNOWN:
      out->set_launch_from(
          ::metrics::CastLogsProto_CastEventProto::FROM_UNKNOWN);
      break;
    case FROM_LOCAL:
      out->set_launch_from(::metrics::CastLogsProto_CastEventProto::FROM_LOCAL);
      break;
    case FROM_DIAL:
      out->set_launch_from(::metrics::CastLogsProto_CastEventProto::FROM_DIAL);
      break;
    case FROM_CAST_V2:
      out->set_launch_from(
          ::metrics::CastLogsProto_CastEventProto::FROM_CAST_V2);
      break;
    case FROM_CCS:
      out->set_launch_from(::metrics::CastLogsProto_CastEventProto::FROM_CCS);
      break;
  }
}

}  // namespace chromecast
