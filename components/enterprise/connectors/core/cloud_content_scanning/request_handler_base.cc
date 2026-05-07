// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/cloud_content_scanning/request_handler_base.h"

#include "base/notreached.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/safe_browsing/core/common/features.h"

namespace enterprise_connectors {

RequestHandlerBase::RequestHandlerBase(
    ContentAnalysisInfoBase* content_analysis_info,
    BinaryUploadService* upload_service,
    GURL url,
    DeepScanAccessPoint access_point)
    : content_analysis_info_(content_analysis_info),
      upload_service_(upload_service ? upload_service->AsWeakPtr() : nullptr),
      url_(url),
      access_point_(access_point) {}

RequestHandlerBase::~RequestHandlerBase() = default;

bool RequestHandlerBase::UploadData() {
  upload_start_time_ = base::TimeTicks::Now();
  return UploadDataImpl();
}

void RequestHandlerBase::AppendFinalActionsTo(
    std::map<std::string, ContentAnalysisAcknowledgement::FinalAction>*
        final_actions) {
  DCHECK(final_actions);
  final_actions->insert(
      std::make_move_iterator(request_tokens_to_ack_final_actions_.begin()),
      std::make_move_iterator(request_tokens_to_ack_final_actions_.end()));

  request_tokens_to_ack_final_actions_.clear();
}

BinaryUploadService* RequestHandlerBase::GetBinaryUploadService() {
  return upload_service_.get();
}

base::TimeTicks RequestHandlerBase::upload_start_time() const {
  return upload_start_time_;
}

DeepScanAccessPoint RequestHandlerBase::access_point() const {
  return access_point_;
}

ContentAnalysisInfoBase* RequestHandlerBase::content_analysis_info() const {
  return content_analysis_info_.get();
}

std::string RequestHandlerBase::access_point_string() const {
  switch (access_point_) {
    case DeepScanAccessPoint::FILE_TRANSFER:
      return kFileTransferDataTransferEventTrigger;
    case DeepScanAccessPoint::UPLOAD:
    case DeepScanAccessPoint::DRAG_AND_DROP:
    case DeepScanAccessPoint::PASTE:
    case DeepScanAccessPoint::ACTOR:
      return kFileUploadDataTransferEventTrigger;
    case DeepScanAccessPoint::DOWNLOAD:
      return kFileDownloadDataTransferEventTrigger;
    case DeepScanAccessPoint::PRINT:
      break;
  }
  NOTREACHED();
}

}  // namespace enterprise_connectors
