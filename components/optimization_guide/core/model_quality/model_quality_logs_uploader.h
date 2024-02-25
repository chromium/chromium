// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOGS_UPLOADER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOGS_UPLOADER_H_

#include <memory>

namespace optimization_guide {

class ModelQualityLogEntry;

// Interface for model quality logging APIs.
class ModelQualityLogsUploader {
 public:
  // Uploads the LogAiDataRequest of the log_entry to the model quality service.
  virtual void UploadModelQualityLogs(
      std::unique_ptr<ModelQualityLogEntry> log_entry) = 0;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_QUALITY_MODEL_QUALITY_LOGS_UPLOADER_H_
