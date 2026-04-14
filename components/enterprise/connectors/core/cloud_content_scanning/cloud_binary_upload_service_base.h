// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_BASE_H_

#include "base/callback_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/resumable_uploader_base.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace enterprise_connectors {

// This class encapsulates the process of uploading a file for deep scanning,
// and asynchronously retrieving a verdict.
//
// TODO(crbug.com/501456247): Change this to inherit from BinaryUploadService
// once the migration is done.
class CloudBinaryUploadServiceBase {
 public:
  explicit CloudBinaryUploadServiceBase();
  virtual ~CloudBinaryUploadServiceBase();

  // The maximum number of uploads that can happen in parallel.
  static size_t GetParallelActiveRequestsMax();

  // Returns the URL that requests are uploaded to. Scans for enterprise go to a
  // different URL than scans for Advanced Protection users and Enhanced
  // Protection users.
  static GURL GetUploadUrl(bool is_consumer_scan_eligible);

  // Returns true if all expected connector results (tags) have been received
  // for the given `request_id`.
  bool ResponseIsComplete(BinaryUploadRequest::Id request_id);

  // Returns the request associated with `request_id`, or nullptr if it doesn't
  // exist in `active_requests_`.
  BinaryUploadRequest* GetRequest(BinaryUploadRequest::Id request_id);

  // If auth check results are available for the matching
  // `authorization_callbacks`, run and clear the callbacks.
  void MaybeRunAuthorizationCallbacks(const std::string& dm_token,
                                      AnalysisConnector connector);

  // Logs the provided information on the chrome://safe-browsing/#tab-deep-scan
  // debug page.
  void LogResponseDebugInfo(const std::string& upload_info,
                            ScanRequestUploadResult result,
                            BinaryUploadRequest* request,
                            const ContentAnalysisResponse& response);

  // TODO(crbug.com/501456247): Change the "protected" below to "private" once
  // the migration is done.
 protected:
  using TokenAndConnector =
      std::pair<std::string, enterprise_connectors::AnalysisConnector>;
  // Auth request class that is used to check if the browser can upload
  // content to the server.
  //
  // TODO(crbug.com/501456247): Move this class to the .cc file, once all the
  // ValidateDataUploadRequest references in CloudBinaryUploadService are moved
  // to the base class.
  class ValidateDataUploadRequest : public BinaryUploadRequest {
   public:
    ValidateDataUploadRequest(
        ContentAnalysisCallback callback,
        CloudAnalysisSettings settings,
        BrowserPolicyConnectorGetter policy_connector_getter);
    ValidateDataUploadRequest(const ValidateDataUploadRequest&) = delete;
    ValidateDataUploadRequest& operator=(const ValidateDataUploadRequest&) =
        delete;
    ~ValidateDataUploadRequest() override;

   private:
    void GetRequestData(DataCallback callback) override;
    bool IsAuthRequest() const override;
  };

  void FinishRequest(BinaryUploadRequest* request,
                     ScanRequestUploadResult result,
                     ContentAnalysisResponse response);

  void FinishAndCleanupRequest(BinaryUploadRequest* request,
                               ScanRequestUploadResult result,
                               ContentAnalysisResponse response);

  // Records UMA metrics for a finished request.
  void RecordRequestMetrics(BinaryUploadRequest::Id request_id,
                            ScanRequestUploadResult result);

  // Records UMA metrics for a finished request, including tag-specific results.
  void RecordRequestMetrics(BinaryUploadRequest::Id request_id,
                            ScanRequestUploadResult result,
                            const ContentAnalysisResponse& response);

  // Tries to start uploads from `request_queue_` depending on the number of
  // currently active requests. This should be called whenever
  // `active_requests_` shrinks so queued requests are started as soon as
  // possible.
  void PopRequestQueue();

  // Upload the given file contents for deep scanning. The results will be
  // returned asynchronously by calling `request`'s `callback`. This must be
  // called on the UI thread.
  virtual void UploadForDeepScanning(
      std::unique_ptr<BinaryUploadRequest> request) = 0;

  // Clears request and associated data from memory and starts the next queued
  // request, if present.
  void CleanupRequest(BinaryUploadRequest* request);

  // Record metrics for the user action duration if this is the last request for
  // the batch cancelled by user corresponding to `action_id`.
  void MaybeTrackUploadUserCancellation(const std::string& action_id);

  bool CheckForUserActionDone(const std::string& action_id);

  void AssertCalledOnUIThread();

  // Convenience callback method that calls both OnGetContentAnalysisResponse
  // and OnContentUploaded. Since the multipart uploader does not send separate
  // requests for metadata and content, it only needs one callback that finishes
  // the request and performs the cleanup.
  void OnUploadComplete(BinaryUploadRequest::Id request_id,
                        bool success,
                        int http_status,
                        const std::string& response_data);

  // Callback that runs when a content analysis verdict is received. Only used
  // explicitly by the resumable uploader.
  void OnGetContentAnalysisResponse(BinaryUploadRequest::Id request_id,
                                    bool success,
                                    int http_status,
                                    const std::string& response_data);

  // Callback to cleanup the request. Only used explicitly by the resumable
  // uploader once the content is uploaded.
  void OnContentUploaded(BinaryUploadRequest::Id request_id);

  void OnGetResponse(BinaryUploadRequest::Id request_id,
                     ContentAnalysisResponse response);

  void MaybeFinishRequest(
      enterprise_connectors::BinaryUploadRequest::Id request_id);

  std::unique_ptr<ConnectorUploadRequest> CreateUploadRequest(
      BinaryUploadRequest* request,
      const BinaryUploadRequest::Id& request_id,
      const GURL& url,
      const std::string& metadata,
      const std::string& histogram_suffix,
      bool force_sync_upload,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      BinaryUploadRequest::Data data,
      ScanRequestUploadResult result,
      ResumableUploadRequestBase::OnceRegisterOnGotHashCallback
          register_on_got_hash_callback);

  // enterprise_connectors::BinaryUploadRequest queued for upload.
  base::circular_deque<
      std::unique_ptr<enterprise_connectors::BinaryUploadRequest>>
      request_queue_;

  // Resources associated with an in-progress request.
  base::flat_map<BinaryUploadRequest::Id, std::unique_ptr<BinaryUploadRequest>>
      active_requests_;
  base::flat_map<BinaryUploadRequest::Id, std::unique_ptr<base::OneShotTimer>>
      active_timers_;
  base::flat_map<BinaryUploadRequest::Id,
                 std::unique_ptr<ConnectorUploadRequest>>
      active_uploads_;
  base::flat_map<enterprise_connectors::BinaryUploadRequest::Id, std::string>
      active_tokens_;

  // Maps request IDs to their start times, used for duration metrics.
  base::flat_map<BinaryUploadRequest::Id, base::TimeTicks> start_times_;

  // Maps requests to each corresponding tag-result pairs.
  base::flat_map<BinaryUploadRequest::Id,
                 base::flat_map<std::string, ContentAnalysisResponse::Result>>
      received_connector_results_;

  // Indicates whether this DM token + Connector combination can be used to
  // upload data for enterprise requests. Advanced Protection scans are
  // validated using the user's Advanced Protection enrollment status.
  base::flat_map<TokenAndConnector,
                 enterprise_connectors::ScanRequestUploadResult>
      can_upload_enterprise_data_;

  // Callbacks waiting on IsAuthorized request. These are organized by DM token
  // and Connector.
  base::flat_map<TokenAndConnector,
                 std::unique_ptr<base::OnceCallbackList<void(
                     enterprise_connectors::ScanRequestUploadResult)>>>
      authorization_callbacks_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Data associated with a user action. Used to track metrics for a user
  // action.
  struct UserActionData {
    bool is_cloud = false;
    enterprise_connectors::DeepScanAccessPoint access_point;
    std::optional<base::TimeTicks> cancelled_time;
  };

  // Tracks the start time and cancellation status for all requests in a user
  // action. Keyed by user action id.
  base::flat_map<std::string, UserActionData> user_action_data_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

 private:
  base::WeakPtrFactory<CloudBinaryUploadServiceBase> weakptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_BASE_H_
