// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_STUDENT_HEARTBEAT_REQUEST_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_STUDENT_HEARTBEAT_REQUEST_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/gaia/gaia_id.h"

namespace google_apis {
class RequestSender;
enum ApiErrorCode;
}  // namespace google_apis

namespace ash::boca {

using StudentHeartbeatCallback = base::OnceCallback<void(
    base::expected<bool, google_apis::ApiErrorCode> result)>;

class StudentHeartbeatRequest : public google_apis::UrlFetchRequestBase {
 public:
  StudentHeartbeatRequest(google_apis::RequestSender* sender,
                          std::string url_base,
                          const std::string& session_id,
                          const GaiaId gaia_id,
                          const std::string& device_id,
                          const std::string& student_group_id,
                          StudentHeartbeatCallback callback);
  StudentHeartbeatRequest(const StudentHeartbeatRequest&) = delete;
  StudentHeartbeatRequest& operator=(const StudentHeartbeatRequest&) = delete;
  ~StudentHeartbeatRequest() override;

  std::string session_id() const { return session_id_; }
  GaiaId gaia_id() const { return gaia_id_; }
  std::string device_id() const { return device_id_; }
  std::string student_group_id() const { return student_group_id_; }

  // For testing.
  void OverrideURLForTesting(const std::string& url);

  // TODO: crbug.com/394633053 Create a helper instead copying callback
  // directly.
  StudentHeartbeatCallback callback() { return std::move(callback_); }

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
  void OnDataParsed(bool success);

  const std::string session_id_;
  const GaiaId gaia_id_;
  const std::string device_id_;
  const std::string student_group_id_;
  std::string url_base_;
  StudentHeartbeatCallback callback_;
  base::WeakPtrFactory<StudentHeartbeatRequest> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SESSION_API_STUDENT_HEARTBEAT_REQUEST_H_
