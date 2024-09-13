// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/test_user_annotations_service.h"

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/user_annotations/user_annotations_types.h"

namespace user_annotations {

TestUserAnnotationsService::TestUserAnnotationsService() = default;
TestUserAnnotationsService::~TestUserAnnotationsService() = default;

void TestUserAnnotationsService::ReplaceAllEntries(
    UserAnnotationsEntries entries) {
  entries_ = std::move(entries);
}

void TestUserAnnotationsService::AddFormSubmission(
    optimization_guide::proto::AXTreeUpdate ax_tree_update,
    const autofill::FormData& form_data,
    ImportFormCallback callback) {
  if (should_import_form_data_) {
    int64_t entry_id = 0;
    for (const autofill::FormFieldData& field : form_data.fields()) {
      optimization_guide::proto::UserAnnotationsEntry entry;
      entry.set_entry_id(entry_id++);
      entry.set_key(base::UTF16ToUTF8(field.label()));
      entry.set_value(base::UTF16ToUTF8(field.value()));
      entries_.emplace_back(std::move(entry));
    }
    std::move(callback).Run(/*to_be_upserted_entries=*/entries_,
                            /*prompt_acceptance_callback=*/base::DoNothing());
    return;
  }
  std::move(callback).Run(/*to_be_upserted_entries=*/{},
                          /*prompt_acceptance_callback=*/base::DoNothing());
}

void TestUserAnnotationsService::RetrieveAllEntries(
    base::OnceCallback<void(UserAnnotationsEntries)> callback) {
  std::move(callback).Run(entries_);
}

void TestUserAnnotationsService::AddHostToFormAnnotationsAllowlist(
    const std::string& host) {
  allowed_forms_annotations_hosts_.insert(host);
}

bool TestUserAnnotationsService::ShouldAddFormSubmissionForURL(
    const GURL& url) {
  return base::Contains(allowed_forms_annotations_hosts_, url.host());
}

void TestUserAnnotationsService::RemoveAnnotationsInRange(
    const base::Time& delete_begin,
    const base::Time& delete_end) {
  last_received_remove_annotations_in_range_ =
      std::make_pair(delete_begin, delete_end);
}

}  // namespace user_annotations
