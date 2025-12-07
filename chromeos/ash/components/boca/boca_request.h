// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_REQUEST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}  // namespace base

namespace google_apis {
class RequestSender;
}  // namespace google_apis

namespace ash::boca {

class BocaRequest : public google_apis::UrlFetchRequestBase {
 public:
  class Delegate {
   public:
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual std::string GetRelativeUrl() = 0;
    virtual std::optional<std::string> GetRequestBody() = 0;
    virtual void OnSuccess(std::unique_ptr<base::Value> response) = 0;
    virtual void OnError(google_apis::ApiErrorCode error) = 0;
    virtual google_apis::HttpRequestMethod GetRequestType() const = 0;

   protected:
    Delegate() = default;
  };

  BocaRequest(google_apis::RequestSender* sender,
              std::unique_ptr<Delegate> delegate);

  BocaRequest(const BocaRequest&) = delete;
  BocaRequest& operator=(const BocaRequest&) = delete;

  ~BocaRequest() override;

 protected:
  // google_apis::UrlFetchRequestBase:
  GURL GetURL() const override;
  google_apis::ApiErrorCode MapReasonToError(
      google_apis::ApiErrorCode code,
      const std::string& reason) override;
  bool IsSuccessfulErrorCode(google_apis::ApiErrorCode error) override;
  google_apis::HttpRequestMethod GetRequestType() const override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(google_apis::ApiErrorCode code) override;

 private:
  void OnDataParsed(std::unique_ptr<base::Value> response_data);

  std::unique_ptr<Delegate> delegate_;
  base::WeakPtrFactory<BocaRequest> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_REQUEST_H_
