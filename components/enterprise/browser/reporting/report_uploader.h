// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_UPLOADER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_UPLOADER_H_

#include <memory>
#include <queue>

#include "base/callback.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/enterprise/browser/reporting/report_request_definition.h"
#include "net/base/backoff_entry.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace policy {
class CloudPolicyClient;
}  // namespace policy

namespace net {
class BackoffEntry;
}  // namespace net

namespace enterprise_reporting {

// A class that is responsible for uploading multiple requests and retrying in
// case of error.
// Requests will be sent one after another with shared retry count. However, a
// successful request will minus the retry count by one.
class ReportUploader {
 public:
  // Request upload result.
  enum ReportStatus {
    kSuccess,
    kTransientError,   // Report can't be uploaded due to transient error like
                       // network error or server side error.
    kPersistentError,  // Report can't be uploaded due to persistent error like
                       // invalid dm token.
  };

  using ReportRequest = definition::ReportRequest;
  using ReportRequests = std::queue<std::unique_ptr<ReportRequest>>;
  // A callback to notify the upload result.
  using ReportCallback = base::OnceCallback<void(ReportStatus status)>;

  ReportUploader(policy::CloudPolicyClient* client,
                 int maximum_number_of_retries);
  virtual ~ReportUploader();

  // Sets a list of requests and upload it. Request will be uploaded one after
  // another.
  virtual void SetRequestAndUpload(ReportRequests requests,
                                   ReportCallback callback);

 private:
  // Uploads the first request in the queue.
  void Upload();

  // Decides retry behavior based on CloudPolicyClient's status for the current
  // request. Or move to the next request.
  void OnRequestFinished(bool status);

  // Retries the first request in the queue.
  void Retry();
  bool HasRetriedTooOften();

  // Notifies the upload result.
  void SendResponse(const ReportStatus status);

  // Moves to the next request if exist, or notifies the accomplishments.
  void NextRequest();

  policy::CloudPolicyClient* client_;
  ReportCallback callback_;
  ReportRequests requests_;

  net::BackoffEntry backoff_entry_;
  base::OneShotTimer backoff_request_timer_;
  const int maximum_number_of_retries_;

  base::WeakPtrFactory<ReportUploader> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ReportUploader);
};

enum ReportResponseMetricsStatus {
  kSuccess = 0,
  kNetworkError = 1,
  kTemporaryServerError = 2,
  kDDSConcurrencyError = 3,
  kRequestTooLargeError = 4,
  kOtherError = 5,
  kMaxValue = kOtherError,
};

}  // namespace enterprise_reporting

#endif  // COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_UPLOADER_H_
