// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_OPEN_METRICS_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_OPEN_METRICS_H_

#include "base/memory/safe_ref.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

// TODO(b/300861997): Add "LogMetric" functions so metrics can be logged through
// this class. Add ability to track the state of each relevant metric in the
// flow and detect inconsistencies.
// Passed through the cloud upload and open flow. Accessed as a `unique_ptr` or
// a SafeRef.
class CloudOpenMetrics {
 public:
  explicit CloudOpenMetrics(CloudProvider cloud_provider);
  ~CloudOpenMetrics();

  // Not copyable. Create a SafeRef instead.
  CloudOpenMetrics(const CloudOpenMetrics&) = delete;
  CloudOpenMetrics& operator=(const CloudOpenMetrics&) = delete;

  // Not movable. Move the `unique_ptr` owning `CloudOpenMetrics` instead.
  CloudOpenMetrics(const CloudOpenMetrics&&) = delete;
  CloudOpenMetrics& operator=(CloudOpenMetrics&&) = delete;

  // Log the `value` for the TransferRequired metric.
  void LogTransferRequired(OfficeFilesTransferRequired value);

  base::SafeRef<CloudOpenMetrics> GetSafeRef() const;

  // For testing.
  base::WeakPtr<CloudOpenMetrics> GetWeakPtr();

 private:
  CloudProvider cloud_provider_;
  base::WeakPtrFactory<CloudOpenMetrics> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_CLOUD_UPLOAD_CLOUD_OPEN_METRICS_H_
