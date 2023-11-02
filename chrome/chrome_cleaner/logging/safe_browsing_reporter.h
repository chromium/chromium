// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_LOGGING_SAFE_BROWSING_REPORTER_H_
#define CHROME_CHROME_CLEANER_LOGGING_SAFE_BROWSING_REPORTER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "chrome/chrome_cleaner/http/http_agent.h"
#include "chrome/chrome_cleaner/http/http_agent_factory.h"
#include "chrome/chrome_cleaner/logging/network_checker.h"
#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace chrome_cleaner {

// Class that uploads serialized protos to Safe Browsing. The upload
// operation will run on a separate thread (via a WorkerPool), and results will
// be posted back on the thread that initially requested the upload.
class SafeBrowsingReporter {
 public:
  // The result of a report upload.
  enum class Result {
    UPLOAD_SUCCESS,           // A response was received.
    UPLOAD_REQUEST_FAILED,    // Upload failed.
    UPLOAD_INVALID_RESPONSE,  // The response was not recognized.
    UPLOAD_TIMED_OUT,         // Upload timed out.
    UPLOAD_INTERNAL_ERROR,    // Internal error.
    UPLOAD_ERROR_TOO_LARGE,   // Entity too large.
    UPLOAD_NO_NETWORK,        // No network or Safe Browsing is not reachable.
    NUM_UPLOAD_RESULTS
  };

  // A callback run by the uploader upon success or failure. The first argument
  // indicates the result of the upload, the second the report that was uploaded
  // while the third contains the response received, if any.
  typedef base::RepeatingCallback<void(Result,
                                       const std::string& serialized_report,
                                       std::unique_ptr<ChromeFoilResponse>)>
      OnResultCallback;

  SafeBrowsingReporter(const SafeBrowsingReporter&) = delete;
  SafeBrowsingReporter& operator=(const SafeBrowsingReporter&) = delete;

  virtual ~SafeBrowsingReporter();

  // Replaces the HttpAgent factory with a new factory. Exposed so tests can
  // create mock HttpAgent objects. Passing an empty factory will reset to the
  // default factory. This method is not thread-safe.
  static void SetHttpAgentFactoryForTesting(const HttpAgentFactory* factory);

  // Sets which Callback should be executed when the code needs to Sleep for a
  // period of time.
  static void SetSleepCallbackForTesting(
      base::RepeatingCallback<void(base::TimeDelta)> callback);

  // Replaces the NetworkChecker with a new instance. Passing a NULL |checker|
  // will reset to the default network checker. Exposed so tests can provide
  // implementations that do not actually check the network presence.
  static void SetNetworkCheckerForTesting(NetworkChecker* checker);

  // Starts the process to upload a report to |default_url|, unless it's
  // overriden by --test-logging-url. |done_callback| will be run when the
  // upload is complete. The callback will always be run (success or failure) on
  // the same thread that was used when this method was called.
  static void UploadReport(
      const OnResultCallback& done_callback,
      const std::string& default_url,
      const std::string& serialized_report,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Cancels all current and future waits, to speed up system shutdown.
  static void CancelWaitForShutdown();

 protected:
  // Initializes SafeBrowsingReporter and posts a task to WorkerPool which will
  // perform the upload on a separate thread. |done_callback| will be run in all
  // cases, on the |done_callback_runner| TaskRunner. Declared protected so
  // tests which override this class can call this.
  SafeBrowsingReporter(
      const OnResultCallback& done_callback,
      const GURL& upload_url,
      const std::string& serialized_report,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<base::TaskRunner> done_callback_runner);

 private:
  // Attempts to upload |serialized_report_|. This method runs on a WorkerPool
  // thread.
  void UploadWithRetry(
      const std::string& serialized_report,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Attempts to upload a report synchronously, retrying up to a fixed number of
  // times on failure.
  Result PerformUploadWithRetries(
      const std::string& serialized_report,
      ChromeFoilResponse* response,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Attempts to upload a report synchronously.
  Result PerformUpload(
      const std::string& serialized_report,
      ChromeFoilResponse* response,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  // Callback to be run when the code needs to sleep for some amount of time.
  static base::RepeatingCallback<void(base::TimeDelta)> sleep_callback_;

  // The URL to upload logs to.
  GURL upload_url_;

  // The TaskRunner on which |done_callback_| must be run.
  scoped_refptr<base::TaskRunner> done_callback_runner_;

  // The callback by which results are returned.
  OnResultCallback done_callback_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_LOGGING_SAFE_BROWSING_REPORTER_H_
