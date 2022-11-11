// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_METRICS_UTIL_H_
#define COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_METRICS_UTIL_H_

#include "ash/public/cpp/desk_template.h"
#include "components/desks_storage/core/desk_model.h"

namespace desks_storage {

// Histogram names for desk templates.
constexpr char kTemplateSizeHistogramName[] = "Ash.DeskTemplate.TemplateSize";

// Histogram names for Save & Recall.
constexpr char kSaveAndRecallTemplateSizeHistogramName[] =
    "Ash.DeskTemplate.SaveAndRecallTemplateSize";

// Wrappers calls base::uma with correct histogram name.
void RecordSavedDeskTemplateSizeHistogram(ash::DeskTemplateType type,
                                          int64_t file_size);

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_LOCAL_DESK_DATA_MANAGER_METRICS_UTIL_H_
