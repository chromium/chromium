// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_BASE_H_
#define COMPONENTS_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_BASE_H_

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/background_service/download_params.h"
#include "content/public/browser/background_fetch_delegate.h"
#include "ui/gfx/image/image.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}

namespace download {
class BackgroundDownloadService;
}  // namespace download

namespace background_fetch {

struct JobDetails;

// Implementation of BackgroundFetchDelegate using the
// BackgroundDownloadService. This base class is shared by multiple embedders,
// with specializations providing their own UI.
class BackgroundFetchDelegateBase : public content::BackgroundFetchDelegate {
 public:
  explicit BackgroundFetchDelegateBase(content::BrowserContext* context);
  BackgroundFetchDelegateBase(const BackgroundFetchDelegateBase&) = delete;
  BackgroundFetchDelegateBase& operator=(const BackgroundFetchDelegateBase&) =
      delete;
  ~BackgroundFetchDelegateBase() override;

  // BackgroundFetchDelegate implementation:
  void GetIconDisplaySize(GetIconDisplaySizeCallback callback) override;
  void CreateDownloadJob(base::WeakPtr<Client> client,
                         std::unique_ptr<content::BackgroundFetchDescription>
                             fetch_description) override;
  void DownloadUrl(const std::string& job_id,
                   const std::string& guid,
                   const std::string& method,
                   const GURL& url,
                   ::network::mojom::CredentialsMode credentials_mode,
                   const net::NetworkTrafficAnnotationTag& traffic_annotation,
                   const net::HttpRequestHeaders& headers,
                   bool has_request_body) override;
  void Abort(const std::string& job_id) override;
  void MarkJobComplete(const std::string& job_id) override;

  // Abort all ongoing downloads and fail the fetch. Currently only used when
  // the bytes downloaded exceed the total download size, if specified.
  void FailFetch(const std::string& job_id, const std::string& download_guid);

  void OnDownloadStarted(
      const std::string& guid,
      std::unique_ptr<content::BackgroundFetchResponse> response);

  void OnDownloadUpdated(const std::string& guid,
                         uint64_t bytes_uploaded,
                         uint64_t bytes_downloaded);

  void OnDownloadFailed(const std::string& guid,
                        std::unique_ptr<content::BackgroundFetchResult> result);

  void OnDownloadSucceeded(
      const std::string& guid,
      std::unique_ptr<content::BackgroundFetchResult> result);

  // Whether the provided GUID is resuming from the perspective of Background
  // Fetch.
  bool IsGuidOutstanding(const std::string& guid) const;

  // Notifies the OfflineContentAggregator of an interrupted download that is
  // in a paused state.
  void RestartPausedDownload(const std::string& download_guid);

  // Returns the set of download GUIDs that have started but did not finish
  // according to Background Fetch. Clears out all references to outstanding
  // GUIDs.
  std::set<std::string> TakeOutstandingGuids();

  // Gets the upload data, if any, associated with the `download_guid`.
  void GetUploadData(const std::string& download_guid,
                     download::GetUploadDataCallback callback);

  base::WeakPtr<BackgroundFetchDelegateBase> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Called in response to UI interactions.
  void PauseDownload(const std::string& job_id);
  void ResumeDownload(const std::string& job_id);
  // |job_id| is passed as a copy since the Abort workflow may invalidate it.
  void CancelDownload(std::string job_id);

  // Called when the UI has finished showing. If `activated` is true, it was
  // tapped, otherwise it was dismissed.
  void OnUiFinished(const std::string& job_id);

  // Called when the UI has been tapped.
  void OnUiActivated(const std::string& job);

 protected:
  // Return the download service for `context_`.
  virtual download::BackgroundDownloadService* GetDownloadService() = 0;

  // Called when a new JobDetails object has been created and inserted in
  // |job_details_map_|.
  virtual void OnJobDetailsCreated(const std::string& job_id) = 0;

  // Called when the UI should first be shown for a Background Fetch job.
  virtual void DoShowUi(const std::string& job_id) = 0;

  // Called when a change to the given job warrants updating the UI.
  virtual void DoUpdateUi(const std::string& job_id) = 0;

  // Called to delete the UI object for a job after the UI is no longer needed.
  virtual void DoCleanUpUi(const std::string& job_id) = 0;

  // Looks up the JobDetails object by `job_id`. `allow_null` defines whether
  // it's OK/expected to not find the job.
  JobDetails* GetJobDetails(const std::string& job_id, bool allow_null = false);

  // Returns the client for a given `job_id`.
  base::WeakPtr<Client> GetClient(const std::string& job_id);

  content::BrowserContext* context() { return context_; }

 private:
  // Starts a download according to `params` belonging to `job_id`.
  void StartDownload(const std::string& job_id,
                     download::DownloadParams params,
                     bool has_request_body);

  void OnDownloadReceived(const std::string& guid,
                          download::DownloadParams::StartResult result);

  void DidGetUploadData(const std::string& job_id,
                        const std::string& download_guid,
                        download::GetUploadDataCallback callback,
                        blink::mojom::SerializedBlobPtr blob);

  raw_ptr<content::BrowserContext> context_;

  // Map from individual download GUIDs to job unique ids.
  std::map<std::string, std::string> download_job_id_map_;

  // Map from job unique ids to the details of the job.
  std::map<std::string, JobDetails> job_details_map_;

  base::WeakPtrFactory<BackgroundFetchDelegateBase> weak_ptr_factory_{this};
};

}  // namespace background_fetch

#endif  // COMPONENTS_BACKGROUND_FETCH_BACKGROUND_FETCH_DELEGATE_BASE_H_
