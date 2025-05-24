// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/student_heartbeat_request.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/gaia/gaia_id.h"

namespace ash::boca {
namespace {

bool ParseResponse(std::string json) {
  // Always notify success if no HTTP error.
  return true;
}
}  // namespace

StudentHeartbeatRequest::StudentHeartbeatRequest(
    google_apis::RequestSender* sender,
    std::string url_base,
    const std::string& session_id,
    const GaiaId gaia_id,
    const std::string& device_id,
    const std::string& student_group_id,
    StudentHeartbeatCallback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      session_id_(std::move(session_id)),
      gaia_id_(std::move(gaia_id)),
      device_id_(std::move(device_id)),
      student_group_id_(std::move(student_group_id)),
      url_base_(std::move(url_base)),
      callback_(std::move(callback)) {}

StudentHeartbeatRequest ::~StudentHeartbeatRequest() = default;

GURL StudentHeartbeatRequest::GetURL() const {
  const auto url = GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
      kStudentHeartbeatUrlTemplate,
      {session_id_, gaia_id_.ToString(), device_id_, student_group_id_},
      /*=offsets*/ nullptr));
  return url;
}

google_apis::ApiErrorCode StudentHeartbeatRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool StudentHeartbeatRequest::IsSuccessfulErrorCode(
    google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

void StudentHeartbeatRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE, base::BindOnce(&ParseResponse, std::move(response_body)),
          base::BindOnce(&StudentHeartbeatRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void StudentHeartbeatRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void StudentHeartbeatRequest::OverrideURLForTesting(const std::string& url) {
  url_base_ = std::move(url);
}
void StudentHeartbeatRequest::OnDataParsed(bool success) {
  std::move(callback_).Run(true);
  OnProcessURLFetchResultsComplete();
}
}  // namespace ash::boca
