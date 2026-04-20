// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_BASE_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_BASE_H_

#include "base/callback_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_cancel_requests.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/connector_upload_request.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/resumable_uploader_base.h"
#include "components/enterprise/connectors/core/common.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace safe_browsing {
class CloudBinaryUploadServiceTest;
}

namespace enterprise_connectors {

// This class encapsulates the process of uploading a file for deep scanning,
// and asynchronously retrieving a verdict.
class CloudBinaryUploadServiceBase : public BinaryUploadService {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Get the access token only if the user matches the management and
    // affiliation requirements.
    virtual void MaybeGetAccessToken(
        BinaryUploadRequest* request,
        base::OnceCallback<void(const std::string&)> access_token_callback) = 0;

    // Returns the browser policy connector for the given request.
    virtual BinaryUploadRequest::BrowserPolicyConnectorGetter
    BrowserPolicyConnectorGetter() = 0;

    // Applicable only for consumer scans.
    virtual bool IsAdvancedProtection() = 0;
    virtual bool IsEnhancedProtection() = 0;

#if BUILDFLAG(IS_CHROMEOS)
    virtual bool IsManagedGuestSession() = 0;
#endif
  };

  CloudBinaryUploadServiceBase(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<Delegate> delegate);
  ~CloudBinaryUploadServiceBase() override;

  // The maximum number of uploads that can happen in parallel.
  static size_t GetParallelActiveRequestsMax();

  // Returns the URL that requests are uploaded to. Scans for enterprise go to a
  // different URL than scans for Advanced Protection users and Enhanced
  // Protection users.
  static GURL GetUploadUrl(bool is_consumer_scan_eligible);

  // BinaryUploadService overrides:
  //
  // Upload the given file contents for deep scanning if the browser is
  // authorized to upload data, otherwise queue the request.
  void MaybeUploadForDeepScanning(
      std::unique_ptr<BinaryUploadRequest> request) override;

  void MaybeAcknowledge(std::unique_ptr<BinaryUploadAck> ack) override;

  // Cancel requests for the given action.
  void MaybeCancelRequests(
      std::unique_ptr<BinaryUploadCancelRequests> cancel) override;

  base::WeakPtr<BinaryUploadService> AsWeakPtr() override;

  // Returns true if all expected connector results (tags) have been received
  // for the given `request_id`.
  bool ResponseIsComplete(BinaryUploadRequest::Id request_id);

  // Returns the request associated with `request_id`, or nullptr if it doesn't
  // exist in `active_requests_`.
  BinaryUploadRequest* GetRequest(BinaryUploadRequest::Id request_id);

  // Indicates whether the DM token/Connector combination is allowed to upload
  // data.
  using AuthorizationCallback =
      base::OnceCallback<void(ScanRequestUploadResult)>;
  void IsAuthorized(const GURL& url,
                    bool per_profile_request,
                    AuthorizationCallback callback,
                    const std::string& dm_token,
                    AnalysisConnector connector);

  // Sets `can_upload_data_` for tests.
  void SetAuthForTesting(const std::string& dm_token,
                         ScanRequestUploadResult auth_check_result);

  Delegate* GetDelegateForTesting() { return delegate_.get(); }

  // TODO(crbug.com/501456247): Change the "protected" below to "private" once
  // the migration is done.
 protected:
  // Upload the given file contents for deep scanning. The results will be
  // returned asynchronously by calling `request`'s `callback`. This must be
  // called on the UI thread.
  //
  // Virtual for testing.
  //
  // TODO(crbug.com/501456247): After the migration is complete, revisit this
  // method to see if we can remove `virtual` while keeping the test coverage.
  virtual void UploadForDeepScanning(
      std::unique_ptr<BinaryUploadRequest> request);

  // This may destroy `request`.
  // Virtual for testing.
  //
  // TODO(crbug.com/501456247): After the migration is complete, revisit this
  // method to see if we can remove `virtual` while keeping the test coverage.
  virtual void OnGetRequestData(BinaryUploadRequest::Id request_id,
                                ScanRequestUploadResult result,
                                BinaryUploadRequest::Data data);

  void FinishRequest(BinaryUploadRequest* request,
                     ScanRequestUploadResult result,
                     ContentAnalysisResponse response);

  void FinishAndCleanupRequest(BinaryUploadRequest* request,
                               ScanRequestUploadResult result,
                               ContentAnalysisResponse response);

 private:
  friend class ::safe_browsing::CloudBinaryUploadServiceTest;
  friend class CloudBinaryUploadServiceBaseTest;
  using TokenAndConnector = std::pair<std::string, AnalysisConnector>;

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

  // Resets `can_upload_data_`. Called every 24 hour by `timer_`.
  void ResetAuthorizationData(const GURL& url);

  // Queue the file for deep scanning. This method should be the only caller of
  // UploadForDeepScanning to avoid consuming too many user resources.
  void QueueForDeepScanning(std::unique_ptr<BinaryUploadRequest> request);

  void FinishIfActive(BinaryUploadRequest::Id request_id,
                      ScanRequestUploadResult result,
                      ContentAnalysisResponse response);

  // Records UMA metrics for a finished request.
  void RecordRequestMetrics(BinaryUploadRequest::Id request_id,
                            ScanRequestUploadResult result);

  // Records UMA metrics for a finished request, including tag-specific results.
  void RecordRequestMetrics(BinaryUploadRequest::Id request_id,
                            ScanRequestUploadResult result,
                            const ContentAnalysisResponse& response);

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

  void MaybeFinishRequest(BinaryUploadRequest::Id request_id);

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

  void MaybeUploadForDeepScanningCallback(
      std::unique_ptr<BinaryUploadRequest> request,
      ScanRequestUploadResult auth_check_result);

  void OnGetAccessToken(BinaryUploadRequest::Id request_id,
                        const std::string& access_token);

  void RegisterOnGotHashCallback(BinaryUploadRequest::Id request_id,
                                 OnGotHashCallback on_got_hash_callback);

  bool ShouldTerminateRequestEarly(BinaryUploadRequest* request,
                                   ScanRequestUploadResult get_data_result,
                                   size_t data_size);

  void CleanupRequest(BinaryUploadRequest* request);

  // Prepares auth and non-auth requests for uploading to the server.
  void PrepareRequestForUpload(BinaryUploadRequest::Id request_id);

  // Set the local IP addresses in the request. This is performed in a separate
  // callback to avoid blocking the UI thread and is only used for enterprise
  // requests.
  void OnIpAddressesFetched(BinaryUploadRequest::Id request_id,
                            std::vector<std::string> ip_addresses);

  std::optional<ScanRequestUploadResult> MaybeGetEnterpriseAuthResult(
      const BinaryUploadRequest& request);

  ScanRequestUploadResult GetConsumerAuthResult(
      const BinaryUploadRequest& request);

  // Callback once the response from the backend is received.
  void ValidateDataUploadRequestConnectorCallback(
      const std::string& dm_token,
      AnalysisConnector connector,
      ScanRequestUploadResult result,
      ContentAnalysisResponse response);

  // Tries to start uploads from `request_queue_` depending on the number of
  // currently active requests. This should be called whenever
  // `active_requests_` shrinks so queued requests are started as soon as
  // possible.
  void PopRequestQueue();

  // BinaryUploadRequest queued for upload.
  base::circular_deque<std::unique_ptr<BinaryUploadRequest>> request_queue_;

  // Resources associated with an in-progress request.
  base::flat_map<BinaryUploadRequest::Id, std::unique_ptr<BinaryUploadRequest>>
      active_requests_;
  base::flat_map<BinaryUploadRequest::Id, std::unique_ptr<base::OneShotTimer>>
      active_timers_;
  base::flat_map<BinaryUploadRequest::Id, std::string> active_tokens_;
  base::flat_map<BinaryUploadRequest::Id,
                 std::unique_ptr<ConnectorUploadRequest>>
      active_uploads_;

  // Maps request IDs to their start times, used for duration metrics.
  base::flat_map<BinaryUploadRequest::Id, base::TimeTicks> start_times_;

  // Maps requests to each corresponding tag-result pairs.
  base::flat_map<BinaryUploadRequest::Id,
                 base::flat_map<std::string, ContentAnalysisResponse::Result>>
      received_connector_results_;

  // Indicates whether this DM token + Connector combination can be used to
  // upload data for enterprise requests. Advanced Protection scans are
  // validated using the user's Advanced Protection enrollment status.
  base::flat_map<TokenAndConnector, ScanRequestUploadResult>
      can_upload_enterprise_data_;

  // Data associated with a user action. Used to track metrics for a user
  // action.
  struct UserActionData {
    bool is_cloud = false;
    DeepScanAccessPoint access_point;
    std::optional<base::TimeTicks> cancelled_time;
  };

  // Tracks the start time and cancellation status for all requests in a user
  // action. Keyed by user action id.
  base::flat_map<std::string, UserActionData> user_action_data_;

  // Ensures we validate the browser is registered with the backend every 24
  // hours.
  base::RepeatingTimer timer_;

  BinaryUploadRequest::Id::Generator request_id_generator_;

  // Indicates if this service is waiting on the backend to validate event
  // reporting. Used to avoid spamming the backend.
  base::flat_set<TokenAndConnector> pending_validate_data_upload_request_;

  // Callbacks waiting on IsAuthorized request. These are organized by DM token
  // and Connector.
  base::flat_map<
      TokenAndConnector,
      std::unique_ptr<base::OnceCallbackList<void(ScanRequestUploadResult)>>>
      authorization_callbacks_;

  std::unique_ptr<Delegate> delegate_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  base::WeakPtrFactory<CloudBinaryUploadServiceBase> weakptr_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_CLOUD_BINARY_UPLOAD_SERVICE_BASE_H_
