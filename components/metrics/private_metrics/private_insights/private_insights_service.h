// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_
#define COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace private_insights {

inline constexpr char kTriggerUploadOutcomeHistogram[] =
    "PrivateMetrics.PrivateInsights.TriggerUploadOutcome";

class COMPONENT_EXPORT(PRIVATE_INSIGHTS) PrivateInsightsService
    : public KeyedService {
 public:
  // LINT.IfChange(PrivateInsightsTriggerUploadOutcome)
  enum class TriggerUploadOutcome {
    kSkippedAlreadyRunning = 0,
    kTaskPosted = 1,
    kMaxValue = kTaskPosted,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/private_metrics/enums.xml:PrivateInsightsTriggerUploadOutcome)

  PrivateInsightsService();
  ~PrivateInsightsService() override;

  PrivateInsightsService(const PrivateInsightsService&) = delete;
  PrivateInsightsService& operator=(const PrivateInsightsService&) = delete;

  void Start();
  void Stop();

  // KeyedService:
  void Shutdown() override;

 private:
  void TriggerUpload();

  // Runs on a background thread pool sequence (allows blocking).
  static bool UploadBlocking();

  void OnUploadComplete(bool result);

  bool is_upload_running_ = false;
  base::RepeatingTimer upload_timer_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PrivateInsightsService> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(PrivateInsightsServiceTest,
                           TriggerUploadSkipsPostingTaskWhenAlreadyRunning);
};

}  // namespace private_insights

#endif  // COMPONENTS_METRICS_PRIVATE_METRICS_PRIVATE_INSIGHTS_PRIVATE_INSIGHTS_SERVICE_H_
