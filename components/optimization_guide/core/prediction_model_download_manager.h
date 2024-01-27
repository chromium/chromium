// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_DOWNLOAD_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_DOWNLOAD_MANAGER_H_

#include <map>
#include <optional>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/download/public/background_service/download_params.h"
#include "components/optimization_guide/core/prediction_model_store.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace download {
class BackgroundDownloadService;
}  // namespace download

namespace optimization_guide {

class PredictionModelDownloadClient;
class PredictionModelDownloadObserver;

namespace proto {
class PredictionModel;
}  // namespace proto

extern const char kPredictionModelOptimizationTargetCustomDataKey[];

// Manages the downloads of prediction models.
// Keep in sync with OptimizationGuidePredictionModelDownloadState in enums.xml.
class PredictionModelDownloadManager {
 public:
  // Callback to get the directory to download models.
  using GetBaseModelDirForDownloadCallback =
      base::RepeatingCallback<base::FilePath(
          proto::OptimizationTarget optimization_target)>;

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
      GetBaseModelDirForDownloadCallback
          get_base_model_dir_for_download_callback,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  virtual ~PredictionModelDownloadManager();
  PredictionModelDownloadManager(const PredictionModelDownloadManager&) =
      delete;
  PredictionModelDownloadManager& operator=(
      const PredictionModelDownloadManager&) = delete;

  // Starts a download for |download_url|.
  virtual void StartDownload(const GURL& download_url,
                             proto::OptimizationTarget optimization_target);

  // Verifies the |download_file_path| came from a trusted source and process
  // the downloaded contents. After verification, creates |base_model_dir|.
  // Returns true on success.
  //
  // Must be called on a background thread, as it performs file I/O.
  static bool VerifyDownload(const base::FilePath& download_file_path,
                             const base::FilePath& base_model_dir,
                             bool delete_file_on_error);

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
      std::optional<proto::OptimizationTarget> optimization_target,
      const std::string& downloaded_guid,
      const base::FilePath& download_file_path);

  // Invoked when the download as specified by |failed_download_guid| failed
  // for |optimization_target|.
  void OnDownloadFailed(
      std::optional<proto::OptimizationTarget> optimization_target,
      const std::string& failed_download_guid);

  // Starts unzipping the contents of |download_file_path|, to |base_model_dir|,
  // when the previous step |is_verify_success| is true.
  void StartUnzipping(proto::OptimizationTarget optimization_target,
                      const base::FilePath& download_file_path,
                      const base::FilePath& base_model_dir,
                      bool is_verify_success);

  // Invoked when the contents of |original_file_path| have been unzipped to
  // |base_model_dir|.
  void OnDownloadUnzipped(proto::OptimizationTarget optimization_target,
                          const base::FilePath& original_file_path,
                          const base::FilePath& base_model_dir,
                          bool success);

  // Processes the contents in |base_model_dir|.
  //
  // Must be called on the background thread, as it performs file I/O. This is a
  // stateless func to avoid needing weird lifetime stuff.
  static std::optional<proto::PredictionModel> ProcessUnzippedContents(
      const base::FilePath& base_model_dir);

  // Notifies |observers_| that a model is ready for |optimization_target|.
  //
  // Must be invoked on the UI thread.
  void NotifyModelReady(proto::OptimizationTarget optimization_target,
                        const base::FilePath& base_model_dir,
                        const std::optional<proto::PredictionModel>& model);

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

  // Callback to get the directory to download models.
  GetBaseModelDirForDownloadCallback get_base_model_dir_for_download_callback_;

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
