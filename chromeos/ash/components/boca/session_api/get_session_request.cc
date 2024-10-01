// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/get_session_request.h"

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/boca_role_util.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/components/boca/session_api/session_parser.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash::boca {

GetSessionRequest::GetSessionRequest(google_apis::RequestSender* sender,
                                     std::string gaia_id,
                                     Callback callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      gaia_id_(std::move(gaia_id)),
      url_base_(kSchoolToolsApiBaseUrl),
      callback_(std::move(callback)) {}

GetSessionRequest::~GetSessionRequest() = default;

void GetSessionRequest::OverrideURLForTesting(std::string url) {
  url_base_ = std::move(url);
}

GURL GetSessionRequest::GetURL() const {
  auto url = GURL(url_base_).Resolve(base::ReplaceStringPlaceholders(
      kGetSessionUrlTemplate, {gaia_id_}, nullptr));
  return url;
}

google_apis::ApiErrorCode GetSessionRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool GetSessionRequest::IsSuccessfulErrorCode(google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

void GetSessionRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  google_apis::ApiErrorCode error = GetErrorCode();
  switch (error) {
    case google_apis::HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&GetSessionProtoFromJson, std::move(response_body)),
          base::BindOnce(&GetSessionRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void GetSessionRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

void GetSessionRequest::OnDataParsed(std::unique_ptr<::boca::Session> session) {
  if (!session) {
    std::move(callback_).Run(base::unexpected(google_apis::PARSE_ERROR));
  } else {
    // Empty session won't be nullptr.
    std::move(callback_).Run(std::move(session));
  }
  OnProcessURLFetchResultsComplete();
}

}  // namespace ash::boca
