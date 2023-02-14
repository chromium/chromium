// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_storage_metrics_util.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"

namespace desks_storage {

const char* GetHistogramName(ash::DeskTemplateType type) {
  switch (type) {
    case ash::DeskTemplateType::kTemplate:
      return kTemplateSizeHistogramName;
    case ash::DeskTemplateType::kSaveAndRecall:
      return kSaveAndRecallTemplateSizeHistogramName;
    case ash::DeskTemplateType::kFloatingWorkspace:
      return kFloatingWorkspaceTemplateSizeHistogramName;
    case ash::DeskTemplateType::kUnknown:
      return nullptr;
  }
}

void RecordSavedDeskTemplateSizeHistogram(ash::DeskTemplateType type,
                                          int64_t file_size) {
  // Record template sizes between ranges of 0 and 100kB. The expected size of
  // `proto_size` is around 8kB.
  if (const char* template_size_metrics_name = GetHistogramName(type)) {
    base::UmaHistogramCounts100000(template_size_metrics_name, file_size);
  }
}

void RecordSavedDeskTemplateSizeHistogram(ash::DeskTemplateType type,
                                          size_t file_size) {
  // Record template sizes between ranges of 0 and 100kB. The expected size of
  // `file_size` is around 8kB.
  if (const char* template_size_metrics_name = GetHistogramName(type)) {
    base::UmaHistogramCounts100000(template_size_metrics_name, file_size);
  }
}

}  // namespace desks_storage
