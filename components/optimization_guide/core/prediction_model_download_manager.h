// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_DOWNLOAD_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_DOWNLOAD_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/download/public/background_service/download_params.h"
#include "components/optimization_guide/core/prediction_model_store.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace download {
class BackgroundDownloadService;
}  // namespace download

namespace optimization_guide {

class PredictionModelDownloadClient;
class PredictionModelDownloadObserver;
class PredictionModelStore;

namespace proto {
class PredictionModel;
}  // namespace proto

extern const char kPredictionModelOptimizationTargetCustomDataKey[];

// Manages the downloads of prediction models.
// Keep in sync with OptimizationGuidePredictionModelDownloadState in enums.xml.
class PredictionModelDownloadManager {
 public:
  // The different states a predition model download goes through.
  enum class PredictionModelDownloadState {
    kUnknown = 0,
    // Model was requested to be downloaded.
    kRequested = 1,
    // Download service started the model download.
    kStarted = 2,

    // Add new values above this line.
    kMaxValue = kStarted,
  };

  PredictionModelDownloadManager(
      download::BackgroundDownloadService* download_service,
      const base::FilePath& models_dir_path,
      PredictionModelStore* prediction_model_store,
      const proto::ModelCacheKey& model_cache_key,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  virtual ~PredictionModelDownloadManager();
  PredictionModelDownloadManager(const PredictionModelDownloadManager&) =
      delete;
  PredictionModelDownloadManager& operator=(
      const PredictionModelDownloadManager&) = delete;

  // Starts a download for |download_url|.
  virtual void StartDownload(const GURL& download_url,
                             proto::OptimizationTarget optimization_target);

  // Verifies the download came from a trusted source and process the downloaded
  // contents. Returns a pair of file paths of the form (src, dst) if
  // |file_path| is successfully verified.
  //
  // Must be called on a background thread, as it performs file I/O.
  static absl::optional<std::pair<base::FilePath, base::FilePath>>
  VerifyDownload(const base::FilePath& file_path, bool delete_file_on_error);

  // Cancels all pending downloads.
  virtual void CancelAllPendingDownloads();

  // Returns whether the downloader can download models.
  virtual bool IsAvailableForDownloads() const;

  // Adds and removes observers.
  //
  // All methods called on observers will be invoked on the UI thread.
  virtual void AddObserver(PredictionModelDownloadObserver* observer);
  virtual void RemoveObserver(PredictionModelDownloadObserver* observer);

 private:
  friend class PredictionModelDownloadClient;
  friend class PredictionModelDownloadManagerTest;

  // Invoked when the Download Service is ready.
  //
  // |pending_download_guids| is the set of GUIDs that were previously scheduled
  // to be downloaded and have still not been downloaded yet.
  // |successful_downloads| is the map from GUID to the file path that it was
  // successfully downloaded to.
  void OnDownloadServiceReady(
      const std::set<std::string>& pending_download_guids,
      const std::map<std::string, base::FilePath>& successful_downloads);

  // Invoked when the Download Service fails to initialize and should not be
  // used for the session.
  void OnDownloadServiceUnavailable();

  // Invoked when the download has been accepted and persisted by the
  // BackgroundDownloadService. The download was requested at
  // |download_requested_time| for |optimization_target|.
  void OnDownloadStarted(proto::OptimizationTarget optimization_target,
                         base::TimeTicks download_requested_time,
                         const std::string& guid,
                         download::DownloadParams::StartResult start_result);

  // Invoked when the download as specified by |downloaded_guid| succeeded for
  // |optimization_target|.
  void OnDownloadSucceeded(
      absl::optional<proto::OptimizationTarget> optimization_target,
      const std::string& downloaded_guid,
      const base::FilePath& file_path);

  // Invoked when the download as specified by |failed_download_guid| failed
  // for |optimization_target|.
  void OnDownloadFailed(
      absl::optional<proto::OptimizationTarget> optimization_target,
      const std::string& failed_download_guid);

  // Starts unzipping the contents of |unzip_paths|, if present. |unzip_paths|
  // is a pair of the form (src, dst), if present.
  void StartUnzipping(
      absl::optional<proto::OptimizationTarget> optimization_target,
      const absl::optional<std::pair<base::FilePath, base::FilePath>>&
          unzip_paths);

  // Invoked when the contents of |original_file_path| have been unzipped to
  // |unzipped_dir_path|.
  void OnDownloadUnzipped(
      absl::optional<proto::OptimizationTarget> optimization_target,
      const base::FilePath& original_file_path,
      const base::FilePath& unzipped_dir_path,
      bool success);

  // Processes the contents in |unzipped_dir_path|.
  //
  // Must be called on the background thread, as it performs file I/O. This is a
  // stateless func to avoid needing weird lifetime stuff.
  static absl::optional<proto::PredictionModel> ProcessUnzippedContents(
      const base::FilePath& model_dir_path,
      const base::FilePath& unzipped_dir_path);

  // Notifies |observers_| that a model is ready for |optimization_target|.
  //
  // Must be invoked on the UI thread.
  void NotifyModelReady(
      absl::optional<proto::OptimizationTarget> optimization_target,
      const base::FilePath& base_model_dir,
      const absl::optional<proto::PredictionModel>& model);

  // Notifies |observers_| that a model download failed for
  // |optimization_target|.
  void NotifyModelDownloadFailed(proto::OptimizationTarget optimization_target);

  // The set of GUIDs that are still pending download.
  std::set<std::string> pending_download_guids_;

  // The Download Service to schedule model downloads with.
  //
  // Guaranteed to outlive |this|.
  raw_ptr<download::BackgroundDownloadService> download_service_;

  // Whether the download service is available.
  bool is_available_for_downloads_;

  // The API key to attach to download requests.
  std::string api_key_;

  // The set of observers to be notified of completed downloads.
  base::ObserverList<PredictionModelDownloadObserver> observers_;

  // Whether the download should be verified. Should only be false for testing.
  bool should_verify_download_ = true;

  // The path to the dir containing models.
  base::FilePath models_dir_path_;

  // The optimization guide model store. Not owned. Should outlive |this|.
  raw_ptr<PredictionModelStore> prediction_model_store_;

  // The ModelCacheKey that the user profile for |this| is associated with.
  const proto::ModelCacheKey model_cache_key_;

  // Background thread where download file processing should be performed.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Sequence checker used to verify all public API methods are called on the
  // UI thread.
  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get weak ptr to self on the UI thread.
  base::WeakPtrFactory<PredictionModelDownloadManager> ui_weak_ptr_factory_{
      this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_DOWNLOAD_MANAGER_H_
