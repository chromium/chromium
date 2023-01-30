// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_UPLOADER_H_
#define COMPONENTS_ENTERPRISE_BROWSER_REPORTING_REPORT_UPLOADER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/enterprise/browser/reporting/report_request.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "net/base/backoff_entry.h"

namespace base {
class OneShotTimer;
}  // namespace base

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

  // A callback to notify the upload result.
  using ReportCallback = base::OnceCallback<void(ReportStatus status)>;

  ReportUploader(policy::CloudPolicyClient* client,
                 int maximum_number_of_retries);

  ReportUploader(const ReportUploader&) = delete;
  ReportUploader& operator=(const ReportUploader&) = delete;

  virtual ~ReportUploader();

  // Sets a list of requests and upload it. Request will be uploaded one after
  // another.
  virtual void SetRequestAndUpload(ReportType report_type,
                                   ReportRequestQueue requests,
                                   ReportCallback callback);

 private:
  // Uploads the first request in the queue.
  void Upload();

  // Decides retry behavior based on CloudPolicyClient's status for the current
  // request. Or move to the next request.
  void OnRequestFinished(policy::CloudPolicyClient::Result result);

  // Retries the first request in the queue.
  void Retry();
  bool HasRetriedTooOften();

  // Notifies the upload result.
  void SendResponse(const ReportStatus status);

  // Moves to the next request if exist, or notifies the accomplishments.
  void NextRequest();

  raw_ptr<policy::CloudPolicyClient> client_;
  ReportCallback callback_;
  ReportRequestQueue requests_;
  ReportType report_type_;

  net::BackoffEntry backoff_entry_;
  base::OneShotTimer backoff_request_timer_;
  const int maximum_number_of_retries_;

  base::WeakPtrFactory<ReportUploader> weak_ptr_factory_{this};
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
