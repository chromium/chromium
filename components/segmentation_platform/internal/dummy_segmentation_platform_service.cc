// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/dummy_segmentation_platform_service.h"

#include <string>

#include "base/threading/sequenced_task_runner_handle.h"
#include "components/segmentation_platform/internal/stats.h"
#include "components/segmentation_platform/public/input_context.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
#include "components/segmentation_platform/public/trigger_context.h"

namespace segmentation_platform {

DummySegmentationPlatformService::DummySegmentationPlatformService() = default;

DummySegmentationPlatformService::~DummySegmentationPlatformService() = default;

void DummySegmentationPlatformService::GetSelectedSegment(
    const std::string& segmentation_key,
    SegmentSelectionCallback callback) {
  stats::RecordSegmentSelectionFailure(
      segmentation_key,
      stats::SegmentationSelectionFailureReason::kPlatformDisabled);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), SegmentSelectionResult()));
}

SegmentSelectionResult DummySegmentationPlatformService::GetCachedSegmentResult(
    const std::string& segmentation_key) {
  return SegmentSelectionResult();
}

void DummySegmentationPlatformService::GetSelectedSegmentOnDemand(
    const std::string& segmentation_key,
    scoped_refptr<InputContext> input_context,
    SegmentSelectionCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), SegmentSelectionResult()));
}

CallbackId
DummySegmentationPlatformService::RegisterOnDemandSegmentSelectionCallback(
    const std::string& segmentation_key,
    const OnDemandSegmentSelectionCallback& callback) {
  return CallbackId::FromUnsafeValue(0);
}

void DummySegmentationPlatformService::
    UnregisterOnDemandSegmentSelectionCallback(
        CallbackId callback_id,
        const std::string& segmentation_key) {}

void DummySegmentationPlatformService::OnTrigger(
    std::unique_ptr<TriggerContext> trigger_context) {}

void DummySegmentationPlatformService::EnableMetrics(
    bool signal_collection_allowed) {}

bool DummySegmentationPlatformService::IsPlatformInitialized() {
  return false;
}

}  // namespace segmentation_platform
