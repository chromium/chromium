// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/public/service_proxy.h"

namespace segmentation_platform {

ServiceProxy::SegmentStatus::SegmentStatus(SegmentId segment_id,
                                           const std::string& segment_metadata,
                                           const std::string& prediction_result,
                                           base::Time prediction_timestamp,
                                           bool can_execute_segment)
    : segment_id(segment_id),
      segment_metadata(segment_metadata),
      prediction_result(prediction_result),
      prediction_timestamp(prediction_timestamp),
      can_execute_segment(can_execute_segment) {}

ServiceProxy::ClientInfo::ClientInfo(const std::string& segmentation_key,
                                     std::optional<SegmentId> selected_segment)
    : segmentation_key(segmentation_key), selected_segment(selected_segment) {}

ServiceProxy::ClientInfo::~ClientInfo() = default;

ServiceProxy::ClientInfo::ClientInfo(const ClientInfo& other) = default;

}  // namespace segmentation_platform