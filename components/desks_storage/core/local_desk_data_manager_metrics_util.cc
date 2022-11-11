// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/local_desk_data_manager_metrics_util.h"

#include "base/metrics/histogram_functions.h"

namespace desks_storage {

void RecordSavedDeskTemplateSizeHistogram(ash::DeskTemplateType type,
                                          int64_t file_size) {
  // Record template sizes between ranges of 0 and 100MB.
  base::UmaHistogramCounts100000(
      type == ash::DeskTemplateType::kTemplate
          ? desks_storage::kTemplateSizeHistogramName
          : desks_storage::kSaveAndRecallTemplateSizeHistogramName,
      file_size);
}

}  // namespace desks_storage
