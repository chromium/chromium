// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOMAIN_RELIABILITY_UPLOADER_H_
#define COMPONENTS_DOMAIN_RELIABILITY_UPLOADER_H_

#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/domain_reliability/domain_reliability_export.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
class URLRequestContextGetter;
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

  typedef base::Callback<void(const UploadResult& result)> UploadCallback;

  DomainReliabilityUploader();

  virtual ~DomainReliabilityUploader();

  // Creates an uploader that uses the given |url_request_context_getter| to
  // get a URLRequestContext to use for uploads. (See test_util.h for a mock
  // version.)
  static std::unique_ptr<DomainReliabilityUploader> Create(
      MockableTime* time,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter);

  // Uploads |report_json| to |upload_url| and calls |callback| when the upload
  // has either completed or failed.
  virtual void UploadReport(const std::string& report_json,
                            int max_beacon_depth,
                            const GURL& upload_url,
                            const UploadCallback& callback) = 0;

  // Shuts down the uploader prior to destruction. Currently, terminates pending
  // uploads and prevents the uploader from starting new ones to avoid hairy
  // lifetime issues at destruction.
  virtual void Shutdown();

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
