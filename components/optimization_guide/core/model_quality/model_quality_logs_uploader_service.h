// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"
#include "url/gurl.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace optimization_guide {

class ModelQualityLogsUploaderService {
 public:
  ModelQualityLogsUploaderService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service);

  ModelQualityLogsUploaderService(const ModelQualityLogsUploaderService&) =
      delete;
  ModelQualityLogsUploaderService& operator=(
      const ModelQualityLogsUploaderService&) = delete;

  ~ModelQualityLogsUploaderService();

  void UploadModelQualityLogs(std::unique_ptr<ModelQualityLogEntry> log_entry);

 private:
  friend class ModelQualityLogsUploaderServiceTest;

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
