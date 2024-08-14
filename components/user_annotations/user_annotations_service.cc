// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_service.h"

#include "base/metrics/histogram_macros_local.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/user_annotations/user_annotations_features.h"
#include "components/user_annotations/user_annotations_types.h"

namespace user_annotations {

UserAnnotationsService::UserAnnotationsService() = default;
UserAnnotationsService::~UserAnnotationsService() = default;

void UserAnnotationsService::AddFormSubmission(
    const optimization_guide::proto::AXTreeUpdate& ax_tree_update,
    const autofill::FormData& form_data) {
  if (ShouldReplaceAnnotationsAfterEachSubmission()) {
    entries_.clear();
  }

  for (const auto& field : form_data.fields()) {
    optimization_guide::proto::UserAnnotationsEntry entry_proto;
    entry_proto.set_key(base::UTF16ToUTF8(
        field.label().empty() ? field.name() : field.label()));
    entry_proto.set_value(base::UTF16ToUTF8(field.value()));
    entries_.push_back({.entry_id = ++entry_id_counter_,
                        .entry_proto = std::move(entry_proto)});
  }
  LOCAL_HISTOGRAM_BOOLEAN("UserAnnotations.DidAddFormSubmission", true);
}

void UserAnnotationsService::RetrieveAllEntries(
    base::OnceCallback<
        void(std::vector<optimization_guide::proto::UserAnnotationsEntry>)>
        callback) {
  std::vector<optimization_guide::proto::UserAnnotationsEntry> entries_protos;
  entries_protos.reserve(entries_.size());
  for (const auto& entry : entries_) {
    entries_protos.push_back(entry.entry_proto);
  }
  std::move(callback).Run(std::move(entries_protos));
}

void UserAnnotationsService::Shutdown() {}

}  // namespace user_annotations
