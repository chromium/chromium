// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "url/gurl.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace optimization_guide {

class MqlsFeatureMetadata;
class ModelQualityLogEntry;

class ModelQualityLogsUploaderService {
 public:
  ModelQualityLogsUploaderService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service);

  ModelQualityLogsUploaderService(const ModelQualityLogsUploaderService&) =
      delete;
  ModelQualityLogsUploaderService& operator=(
      const ModelQualityLogsUploaderService&) = delete;

  virtual ~ModelQualityLogsUploaderService();

  // Does various checks like metrics consent, enterprise check before uploading
  // the logs.
  virtual bool CanUploadLogs(const MqlsFeatureMetadata* metadata);

  // Sets system metadata, including the UMA system profile.
  virtual void SetSystemMetadata(proto::LoggingMetadata* logging_metadata);

  // Returns the WeakPtr for uploading logs during model qualtiy logs
  // destruction.
  base::WeakPtr<ModelQualityLogsUploaderService> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Test-only setter. Pairs well with TestUrlLoaderFactory.
  void SetUrlLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

 protected:
  virtual void UploadFinalizedLog(std::unique_ptr<proto::LogAiDataRequest> log,
                                  proto::LogAiDataRequest::FeatureCase feature);

 private:
  friend class ModelQualityLogsUploaderServiceTest;
  friend class ModelQualityLogEntry;

  void UploadModelQualityLogs(
      std::unique_ptr<proto::LogAiDataRequest> log_ai_data_request);

  // The URL for the remote model quality logs uploader service.
  const GURL model_quality_logs_uploader_service_url_;

  // A weak pointer to the PrefService used to read and write preferences.
  raw_ptr<PrefService> pref_service_;

  // Used for creating an active_url_loader when needed for request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get `weak_ptr_` to self.
  base::WeakPtrFactory<ModelQualityLogsUploaderService> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_
