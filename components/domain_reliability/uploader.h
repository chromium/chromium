// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_UPLOADER_H_
#define COMPONENTS_DOMAIN_RELIABILITY_UPLOADER_H_

#include <map>
#include <memory>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "url/gurl.h"

namespace net {
class IsolationInfo;
class URLRequest;
class URLRequestContext;
}  // namespace net

namespace domain_reliability {

class MockableTime;

// Uploads Domain Reliability reports to collectors.
class DOMAIN_RELIABILITY_EXPORT DomainReliabilityUploader {
 public:
  struct UploadResult {
    enum UploadStatus {
      FAILURE,
      SUCCESS,
      RETRY_AFTER,
    };

    bool is_success() const { return status == SUCCESS; }
    bool is_failure() const { return status == FAILURE; }
    bool is_retry_after() const { return status == RETRY_AFTER; }

    UploadStatus status;
    base::TimeDelta retry_after;
  };

  using UploadCallback = base::OnceCallback<void(const UploadResult& result)>;

  DomainReliabilityUploader();

  virtual ~DomainReliabilityUploader();

  // Creates an uploader that uses the given |url_request_context| for uploads.
  // (See test_util.h for a mock version.)
  static std::unique_ptr<DomainReliabilityUploader> Create(
      MockableTime* time,
      net::URLRequestContext* url_request_context);

  // Uploads |report_json| to |upload_url| and calls |callback| when the upload
  // has either completed or failed.
  virtual void UploadReport(const std::string& report_json,
                            int max_beacon_depth,
                            const GURL& upload_url,
                            const net::IsolationInfo& isolation_info,
                            UploadCallback callback) = 0;

  // Shuts down the uploader prior to destruction. Currently, terminates pending
  // uploads and prevents the uploader from starting new ones to avoid hairy
  // lifetime issues at destruction.
  virtual void Shutdown() = 0;

  // Sets whether the uploader will discard uploads but pretend they succeeded.
  // In Chrome, this is used when the user has not opted in to metrics
  // collection; in unittests, this is used in combination with
  // GetDiscardedUploadCount to simplify checking whether a test scenario
  // generates an upload or not.
  virtual void SetDiscardUploads(bool discard_uploads) = 0;

  // Gets the number of uploads that have been discarded after SetDiscardUploads
  // was called with true.
  virtual int GetDiscardedUploadCount() const = 0;

  static int GetURLRequestUploadDepth(const net::URLRequest& request);
};

}  // namespace domain_reliability

#endif  // COMPONENTS_DOMAIN_RELIABILITY_UPLOADER_H_
