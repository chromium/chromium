// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_GET_SESSION_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_GET_SESSION_REQUEST_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/common/base_requests.h"

namespace boca {
class Session;
}

namespace ash::boca {
class GetSessionRequest : public google_apis::UrlFetchRequestBase {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<::boca::Session>,
                     google_apis::ApiErrorCode> result)>;

  GetSessionRequest(google_apis::RequestSender* sender,
                    std::string gaia_id,
                    Callback callback);
  GetSessionRequest(const GetSessionRequest&) = delete;
  GetSessionRequest& operator=(const GetSessionRequest&) = delete;
  ~GetSessionRequest() override;

  // For testing.
  void OverrideURLForTesting(std::string url);
  Callback callback() { return std::move(callback_); }

 protected:
  // UrlFetchRequestBase:
  GURL GetURL() const override;
  google_apis::ApiErrorCode MapReasonToError(
      google_apis::ApiErrorCode code,
      const std::string& reason) override;
  bool IsSuccessfulErrorCode(google_apis::ApiErrorCode error) override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      const base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(google_apis::ApiErrorCode code) override;

 private:
  void OnDataParsed(std::unique_ptr<::boca::Session> session);
  std::string gaia_id_;
  std::string url_base_;
  Callback callback_;

  base::WeakPtrFactory<GetSessionRequest> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_GET_SESSION_REQUEST_H_
