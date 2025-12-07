// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_request.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "chromeos/ash/components/boca/util.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash::boca {

BocaRequest::BocaRequest(google_apis::RequestSender* sender,
                         std::unique_ptr<Delegate> delegate)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      delegate_(std::move(delegate)) {}

BocaRequest::~BocaRequest() = default;

GURL BocaRequest::GetURL() const {
  return GURL(ash::boca::GetSchoolToolsUrl())
      .Resolve(delegate_->GetRelativeUrl());
}

google_apis::ApiErrorCode BocaRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool BocaRequest::IsSuccessfulErrorCode(google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

google_apis::HttpRequestMethod BocaRequest::GetRequestType() const {
  return delegate_->GetRequestType();
}

bool BocaRequest::GetContentData(std::string* upload_content_type,
                                 std::string* upload_content) {
  std::optional<std::string> request_body = delegate_->GetRequestBody();
  if (!request_body.has_value()) {
    return false;
  }
  *upload_content_type = "application/json";
  *upload_content = request_body.value();
  return true;
}

void BocaRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  if (error != google_apis::HTTP_SUCCESS) {
    delegate_->OnError(error);
    OnProcessURLFetchResultsComplete();
    return;
  }
  blocking_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&google_apis::ParseJson, std::move(response_body)),
      base::BindOnce(&BocaRequest::OnDataParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BocaRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode code) {
  delegate_->OnError(code);
}

void BocaRequest::OnDataParsed(std::unique_ptr<base::Value> response_data) {
  if (!response_data) {
    delegate_->OnError(google_apis::PARSE_ERROR);
  } else {
    delegate_->OnSuccess(std::move(response_data));
  }
  OnProcessURLFetchResultsComplete();
}

}  // namespace ash::boca
