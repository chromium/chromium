// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_download_manager.h"

#include "base/containers/flat_set.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/crx_file/crx_verifier.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/prediction_model_download_observer.h"
#include "components/optimization_guide/core/prediction_model_store.h"
#include "components/services/unzip/public/cpp/unzip.h"
#include "crypto/sha2.h"
#include "google_apis/common/api_key_request_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

#if BUILDFLAG(IS_IOS)
#include "components/services/unzip/in_process_unzipper.h"  // nogncheck
#else
#include "components/services/unzip/content/unzip_service.h"  // nogncheck
#endif

namespace optimization_guide {

namespace {

// The SHA256 hash of the public key for the Optimization Guide Server that
// we require models to come from.
constexpr uint8_t kPublisherKeyHash[] = {
    0x66, 0xa1, 0xd9, 0x3e, 0x4e, 0x5a, 0x66, 0x8a, 0x0f, 0xd3, 0xfa,
    0xa3, 0x70, 0x71, 0x42, 0x16, 0x0d, 0x2d, 0x68, 0xb0, 0x53, 0x02,
    0x5c, 0x7f, 0xd0, 0x0c, 0xa1, 0x6e, 0xef, 0xdd, 0x63, 0x7a};
const net::NetworkTrafficAnnotationTag
    kOptimizationGuidePredictionModelsTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("optimization_guide_model_download",
                                            R"(
        semantics {
          sender: "Optimization Guide"
          description:
            "Chromium interacts with Optimization Guide Service to download "
            "non-personalized models used to improve browser behavior around "
            "page load performance and features such as Translate."
          trigger:
            "When there are new models to download based on response from "
            "Optimization Guide Service that is triggered at the start of each "
            "session."
          data: "The URL provided by the Optimization Guide Service to fetch "
            "an updated model. No user information is sent."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled."
          chrome_policy {
            ComponentUpdatesEnabled {
              policy_options {mode: MANDATORY}
              ComponentUpdatesEnabled: false
            }
          }
        })");

void RecordPredictionModelDownloadStatus(PredictionModelDownloadStatus status) {
  base::UmaHistogramEnumeration(
      "OptimizationGuide.PredictionModelDownloadManager."
      "DownloadStatus",
      status);
}

// Writes the |model_info| to |file_path|.
bool WriteModelInfoProtoToFile(const proto::ModelInfo& model_info,
                               const base::FilePath& file_path) {
  std::string model_info_str;
  if (!model_info.SerializeToString(&model_info_str)) {
    return false;
  }
  return base::WriteFile(file_path, model_info_str);
}

}  // namespace

const char kPredictionModelOptimizationTargetCustomDataKey[] =
    "PredictionModelOptimizationTargetCustomDataKey";

PredictionModelDownloadManager::PredictionModelDownloadManager(
    download::BackgroundDownloadService* download_service,
    GetBaseModelDirForDownloadCallback get_base_model_dir_for_download_callback,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : download_service_(download_service),
      is_available_for_downloads_(true),
      api_key_(features::GetOptimizationGuideServiceAPIKey()),
      get_base_model_dir_for_download_callback_(
          get_base_model_dir_for_download_callback),
      background_task_runner_(background_task_runner) {}

PredictionModelDownloadManager::~PredictionModelDownloadManager() = default;

void PredictionModelDownloadManager::StartDownload(
    const GURL& download_url,
    proto::OptimizationTarget optimization_target) {
  download::DownloadParams download_params;
  download_params.client =
      download::DownloadClient::OPTIMIZATION_GUIDE_PREDICTION_MODELS;
  download_params.guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  download_params.custom_data[kPredictionModelOptimizationTargetCustomDataKey] =
      proto::OptimizationTarget_Name(optimization_target);
  download_params.callback =
      base::BindRepeating(&PredictionModelDownloadManager::OnDownloadStarted,
                          ui_weak_ptr_factory_.GetWeakPtr(),
                          optimization_target, base::TimeTicks::Now());
  download_params.traffic_annotation = net::MutableNetworkTrafficAnnotationTag(
      kOptimizationGuidePredictionModelsTrafficAnnotation);
  // The downloaded models are all Google-generated, so bypass the safety
  // checks.
  download_params.request_params.require_safety_checks = false;
  download_params.request_params.url = download_url;
  download_params.request_params.method = "GET";
  google_apis::AddAPIKeyToRequest(
      download_params.request_params.request_headers, api_key_);
  if (features::IsUnrestrictedModelDownloadingEnabled()) {
    // This feature param should really only be used for testing, so it is ok
    // to have this be a high priority download with no network restrictions.
    download_params.scheduling_params.priority =
        download::SchedulingParams::Priority::HIGH;
  } else {
    download_params.scheduling_params.priority =
        download::SchedulingParams::Priority::NORMAL;
  }
  download_params.scheduling_params.battery_requirements =
      download::SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
  download_params.scheduling_params.network_requirements =
      download::SchedulingParams::NetworkRequirements::NONE;
  base::UmaHistogramEnumeration(
      "OptimizationGuide.PredictionModelDownloadManager.State." +
          optimization_guide::GetStringNameForOptimizationTarget(
              optimization_target),
      PredictionModelDownloadManager::PredictionModelDownloadState::kRequested);

  download_service_->StartDownload(std::move(download_params));
}

void PredictionModelDownloadManager::CancelAllPendingDownloads() {
  for (const std::string& pending_download_guid : pending_download_guids_) {
    download_service_->CancelDownload(pending_download_guid);
  }
}

bool PredictionModelDownloadManager::IsAvailableForDownloads() const {
  return is_available_for_downloads_;
}

void PredictionModelDownloadManager::AddObserver(
    PredictionModelDownloadObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.AddObserver(observer);
}

void PredictionModelDownloadManager::RemoveObserver(
    PredictionModelDownloadObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.RemoveObserver(observer);
}

void PredictionModelDownloadManager::OnDownloadServiceReady(
    const std::set<std::string>& pending_download_guids,
    const std::map<std::string, base::FilePath>& successful_downloads) {
  for (const std::string& pending_download_guid : pending_download_guids) {
    pending_download_guids_.insert(pending_download_guid);
  }

  // Successful downloads should already be notified via |onDownloadSucceeded|,
  // so we don't do anything with them here.
}

void PredictionModelDownloadManager::OnDownloadServiceUnavailable() {
  is_available_for_downloads_ = false;
}

void PredictionModelDownloadManager::OnDownloadStarted(
    proto::OptimizationTarget optimization_target,
    base::TimeTicks download_requested_time,
    const std::string& guid,
    download::DownloadParams::StartResult start_result) {
  if (start_result == download::DownloadParams::StartResult::ACCEPTED) {
    pending_download_guids_.insert(guid);
    base::UmaHistogramEnumeration(
        "OptimizationGuide.PredictionModelDownloadManager.State." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target),
        PredictionModelDownloadManager::PredictionModelDownloadState::kStarted);
    base::UmaHistogramLongTimes(
        "OptimizationGuide.PredictionModelDownloadManager."
        "DownloadStartLatency." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target),
        base::TimeTicks::Now() - download_requested_time);
    for (PredictionModelDownloadObserver& observer : observers_) {
      observer.OnModelDownloadStarted(optimization_target);
    }
  }
}

void PredictionModelDownloadManager::OnDownloadSucceeded(
    std::optional<proto::OptimizationTarget> optimization_target,
    const std::string& guid,
    const base::FilePath& download_file_path) {
  pending_download_guids_.erase(guid);

  base::UmaHistogramBoolean(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadSucceeded",
      optimization_target.has_value());
  if (!optimization_target) {
    return;
  }

  base::FilePath base_model_dir =
      get_base_model_dir_for_download_callback_.Run(*optimization_target);

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PredictionModelDownloadManager::VerifyDownload,
                     download_file_path, base_model_dir,
                     /*delete_file_on_error=*/true),
      base::BindOnce(&PredictionModelDownloadManager::StartUnzipping,
                     ui_weak_ptr_factory_.GetWeakPtr(), *optimization_target,
                     download_file_path, base_model_dir));
}

void PredictionModelDownloadManager::OnDownloadFailed(
    std::optional<proto::OptimizationTarget> optimization_target,
    const std::string& guid) {
  pending_download_guids_.erase(guid);

  base::UmaHistogramBoolean(
      "OptimizationGuide.PredictionModelDownloadManager.DownloadSucceeded",
      false);
  if (optimization_target) {
    NotifyModelDownloadFailed(*optimization_target);
  }
}

// static
// Note: This function runs on a background sequence!
bool PredictionModelDownloadManager::VerifyDownload(
    const base::FilePath& download_file_path,
    const base::FilePath& base_model_dir,
    bool delete_file_on_error) {
  if (!switches::ShouldSkipModelDownloadVerificationForTesting()) {
    // Verify that the |download_file_path| contains a valid CRX file.
    std::string public_key;
    crx_file::VerifierResult verifier_result = crx_file::Verify(
        download_file_path, crx_file::VerifierFormat::CRX3,
        /*required_key_hashes=*/{},
        /*required_file_hash=*/{}, &public_key,
        /*crx_id=*/nullptr, /*compressed_verified_contents=*/nullptr);
    if (verifier_result != crx_file::VerifierResult::OK_FULL) {
      RecordPredictionModelDownloadStatus(
          PredictionModelDownloadStatus::kFailedCrxVerification);
      if (delete_file_on_error) {
        base::ThreadPool::PostTask(
            FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
            base::GetDeleteFileCallback(download_file_path));
      }
      return false;
    }

    // Verify that the CRX3 file is from a publisher we trust.
    std::vector<uint8_t> publisher_key_hash(std::begin(kPublisherKeyHash),
                                            std::end(kPublisherKeyHash));

    std::vector<uint8_t> public_key_hash(crypto::kSHA256Length);
    crypto::SHA256HashString(public_key, public_key_hash.data(),
                             public_key_hash.size());

    if (publisher_key_hash != public_key_hash) {
      RecordPredictionModelDownloadStatus(
          PredictionModelDownloadStatus::kFailedCrxInvalidPublisher);
      if (delete_file_on_error) {
        base::ThreadPool::PostTask(
            FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
            base::GetDeleteFileCallback(download_file_path));
      }
      return false;
    }
  }

  if (base_model_dir.empty()) {
    RecordPredictionModelDownloadStatus(
        PredictionModelDownloadStatus::kOptGuideDirectoryDoesNotExist);
    return false;
  }

  if (!base::CreateDirectory(base_model_dir)) {
    RecordPredictionModelDownloadStatus(
        PredictionModelDownloadStatus::kCouldNotCreateDirectory);
    return false;
  }

  return true;
}

void PredictionModelDownloadManager::StartUnzipping(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& download_file_path,
    const base::FilePath& base_model_dir,
    bool is_verify_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!is_verify_success) {
    NotifyModelDownloadFailed(optimization_target);
    return;
  }

#if BUILDFLAG(IS_IOS)
  auto unzipper = unzip::LaunchInProcessUnzipper();
#else
  auto unzipper = unzip::LaunchUnzipper();
#endif
  unzip::Unzip(
      std::move(unzipper), download_file_path, base_model_dir,
      unzip::mojom::UnzipOptions::New(), unzip::AllContents(),
      base::DoNothing(),
      base::BindOnce(&PredictionModelDownloadManager::OnDownloadUnzipped,
                     ui_weak_ptr_factory_.GetWeakPtr(), optimization_target,
                     download_file_path, base_model_dir));
}

void PredictionModelDownloadManager::OnDownloadUnzipped(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& original_file_path,
    const base::FilePath& base_model_dir,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clean up original download file when this function finishes.
  background_task_runner_->PostTask(
      FROM_HERE, base::GetDeleteFileCallback(original_file_path));

  if (!success) {
    NotifyModelDownloadFailed(optimization_target);
    RecordPredictionModelDownloadStatus(
        PredictionModelDownloadStatus::kFailedCrxUnzip);
    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&PredictionModelDownloadManager::ProcessUnzippedContents,
                     base_model_dir),
      base::BindOnce(&PredictionModelDownloadManager::NotifyModelReady,
                     ui_weak_ptr_factory_.GetWeakPtr(), optimization_target,
                     base_model_dir));
}

// static
std::optional<proto::PredictionModel>
PredictionModelDownloadManager::ProcessUnzippedContents(
    const base::FilePath& base_model_dir) {
  // Unpack and verify model info file.
  base::FilePath model_info_path =
      base_model_dir.Append(GetBaseFileNameForModelInfo());
  std::string binary_model_info_pb;
  if (!base::ReadFileToString(model_info_path, &binary_model_info_pb)) {
    RecordPredictionModelDownloadStatus(
        PredictionModelDownloadStatus::kFailedModelInfoFileRead);
    return std::nullopt;
  }
  proto::ModelInfo model_info;
  if (!model_info.ParseFromString(binary_model_info_pb)) {
    RecordPredictionModelDownloadStatus(
        PredictionModelDownloadStatus::kFailedModelInfoParsing);
    return std::nullopt;
  }
  if (!model_info.has_version() || !model_info.has_optimization_target()) {
    RecordPredictionModelDownloadStatus(
        PredictionModelDownloadStatus::kFailedModelInfoInvalid);
    return std::nullopt;
  }

  base::FilePath model_path = base_model_dir.Append(GetBaseFileNameForModels());
  if (!base::PathExists(model_path)) {
    RecordPredictionModelDownloadStatus(
        PredictionModelDownloadStatus::kFailedModelFileNotFound);
    return std::nullopt;
  }
  proto::PredictionModel model;
  *model.mutable_model_info() = model_info;
  model.mutable_model()->set_download_url(FilePathToString(model_path));

  // Clear all the additional files set by the server so they they can be fully
  // replaced by absolute paths.
  model.mutable_model_info()->clear_additional_files();

  for (const auto& additional_file : model_info.additional_files()) {
    base::FilePath store_add_file_path =
        base_model_dir.AppendASCII(additional_file.file_path());
    if (!base::PathExists(store_add_file_path)) {
      RecordPredictionModelDownloadStatus(
          PredictionModelDownloadStatus::kFailedInvalidAdditionalFile);
      return std::nullopt;
    }
    model.mutable_model_info()->add_additional_files()->set_file_path(
        FilePathToString(store_add_file_path));
  }

  // Save the model info with relative paths.
  if (!WriteModelInfoProtoToFile(model_info, model_info_path)) {
    RecordPredictionModelDownloadStatus(
        PredictionModelDownloadStatus::kFailedModelInfoSaving);
    return std::nullopt;
  }
  RecordPredictionModelDownloadStatus(PredictionModelDownloadStatus::kSuccess);

  return std::make_optional(model);
}

void PredictionModelDownloadManager::NotifyModelReady(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& base_model_dir,
    const std::optional<proto::PredictionModel>& model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!model) {
    // Clean up model dir.
    background_task_runner_->PostTask(
        FROM_HERE, base::GetDeletePathRecursivelyCallback(base_model_dir));
    NotifyModelDownloadFailed(optimization_target);
    return;
  }

  for (PredictionModelDownloadObserver& observer : observers_) {
    observer.OnModelReady(base_model_dir, *model);
  }
}

void PredictionModelDownloadManager::NotifyModelDownloadFailed(
    proto::OptimizationTarget optimization_target) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (PredictionModelDownloadObserver& observer : observers_) {
    observer.OnModelDownloadFailed(optimization_target);
  }
}

}  // namespace optimization_guide
