// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/private_insights/private_insights_service.h"

#include <string>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/metrics/private_metrics/private_insights/private_insights_features.h"

namespace private_insights {

PrivateInsightsService::PrivateInsightsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

PrivateInsightsService::~PrivateInsightsService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PrivateInsightsService::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (upload_timer_.IsRunning()) {
    return;
  }
  base::TimeDelta interval = kPrivateInsightsUploadInterval.Get();
  if (interval.is_positive()) {
    upload_timer_.Start(
        FROM_HERE, interval,
        base::BindRepeating(&PrivateInsightsService::TriggerUpload,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void PrivateInsightsService::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  upload_timer_.Stop();
}

void PrivateInsightsService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Stop();
}

void PrivateInsightsService::TriggerUpload() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_upload_running_) {
    base::UmaHistogramEnumeration(kTriggerUploadOutcomeHistogram,
                                  TriggerUploadOutcome::kSkippedAlreadyRunning);
    return;
  }
  is_upload_running_ = true;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&PrivateInsightsService::UploadBlocking,
                     base::TimeTicks::Now()),
      base::BindOnce(&PrivateInsightsService::OnUploadComplete,
                     weak_ptr_factory_.GetWeakPtr()));

  base::UmaHistogramEnumeration(kTriggerUploadOutcomeHistogram,
                                TriggerUploadOutcome::kTaskPosted);
}

// static
bool PrivateInsightsService::UploadBlocking(base::TimeTicks trigger_time) {
  base::UmaHistogramTimes(kUploadPendingTimeHistogram,
                          base::TimeTicks::Now() - trigger_time);
  base::TimeTicks upload_start_time = base::TimeTicks::Now();

  // TODO(b/518646350): Add the RunFederatedComputation call here.
  bool result = true;

  base::UmaHistogramTimes(kUploadTimeHistogram,
                          base::TimeTicks::Now() - upload_start_time);
  return result;
}

void PrivateInsightsService::OnUploadComplete(bool _result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_upload_running_ = false;
  // TODO(b/518646350): Handle the result of the upload.
}

}  // namespace private_insights
