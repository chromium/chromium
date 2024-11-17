// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_JOIN_SESSION_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_JOIN_SESSION_REQUEST_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "google_apis/common/base_requests.h"

namespace google_apis {
class RequestSender;
enum ApiErrorCode;
}  // namespace google_apis

namespace ash::boca {

//=================JoinSessionRequest================

using JoinSessionCallback =
    base::OnceCallback<void(base::expected<std::unique_ptr<::boca::Session>,
                                           google_apis::ApiErrorCode> result)>;

class JoinSessionRequest : public google_apis::UrlFetchRequestBase {
 public:
  JoinSessionRequest(google_apis::RequestSender* sender,
                     ::boca::UserIdentity user,
                     std::string device_id,
                     std::string join_code,
                     JoinSessionCallback callback);
  JoinSessionRequest(const JoinSessionRequest&) = delete;
  JoinSessionRequest& operator=(const JoinSessionRequest&) = delete;
  ~JoinSessionRequest() override;

  // For testing.
  void OverrideURLForTesting(std::string url);

  JoinSessionCallback callback() { return std::move(callback_); }

  ::boca::UserIdentity& gaia_id() { return user_; }
  std::string device_id() { return device_id_; }

 protected:
  // UrlFetchRequestBase:
  google_apis::HttpRequestMethod GetRequestType() const override;
  GURL GetURL() const override;
  google_apis::ApiErrorCode MapReasonToError(
      google_apis::ApiErrorCode code,
      const std::string& reason) override;
  bool IsSuccessfulErrorCode(google_apis::ApiErrorCode error) override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      const base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(google_apis::ApiErrorCode code) override;

 private:
  void OnDataParsed(std::unique_ptr<::boca::Session> session);
  std::string join_code_;
  ::boca::UserIdentity user_;
  std::string device_id_;

  std::string url_base_;
  JoinSessionCallback callback_;

  base::WeakPtrFactory<JoinSessionRequest> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_JOIN_SESSION_REQUEST_H_
