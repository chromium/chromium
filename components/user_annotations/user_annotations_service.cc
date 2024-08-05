// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include "base/metrics/histogram_macros_local.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/user_annotations/user_annotations_types.h"

namespace user_annotations {

UserAnnotationsService::UserAnnotationsService() = default;
UserAnnotationsService::~UserAnnotationsService() = default;

void UserAnnotationsService::AddFormSubmission(
    const optimization_guide::proto::ComposeAXTreeUpdate& ax_tree_update,
    const autofill::FormData& form_data) {
  for (const auto& field : form_data.fields()) {
    entries_.push_back(
        {.entry_id = ++entry_id_counter_,
         .key = field.label().empty() ? field.name() : field.label(),
         .value = field.value()});
  }
  LOCAL_HISTOGRAM_BOOLEAN("UserAnnotations.DidAddFormSubmission", true);
}

void UserAnnotationsService::RetrieveAllEntries(
    base::OnceCallback<void(std::vector<Entry>)> callback) {
  std::move(callback).Run(entries_);
}

void UserAnnotationsService::Shutdown() {}

}  // namespace user_annotations
