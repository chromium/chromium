// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/test_user_annotations_service.h"

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/form_structure.h"
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

void TestUserAnnotationsService::RemoveEntry(EntryID entry_id,
                                             base::OnceClosure callback) {
  size_t count = 0;
  for (const optimization_guide::proto::UserAnnotationsEntry& entry :
       entries_) {
    if (entry_id == entry.entry_id()) {
      entries_.erase(entries_.begin() + count);
      break;
    }
    count++;
  }
  std::move(callback).Run();
}

void TestUserAnnotationsService::RemoveAllEntries(base::OnceClosure callback) {
  entries_.clear();
  std::move(callback).Run();
}

void TestUserAnnotationsService::AddFormSubmission(
    const GURL& url,
    const std::string& title,
    optimization_guide::proto::AXTreeUpdate ax_tree_update,
    std::unique_ptr<autofill::FormStructure> form,
    ImportFormCallback callback) {
  if (should_import_form_data_) {
    int64_t entry_id = 0;
    for (const std::unique_ptr<autofill::AutofillField>& field :
         form->fields()) {
      optimization_guide::proto::UserAnnotationsEntry entry;
      entry.set_entry_id(entry_id++);
      entry.set_key(base::UTF16ToUTF8(field->label()));
      entry.set_value(
          base::UTF16ToUTF8(field->value(autofill::ValueSemantics::kCurrent)));
      entries_.emplace_back(std::move(entry));
    }
    std::move(callback).Run(std::move(form),
                            /*to_be_upserted_entries=*/entries_,
                            /*prompt_acceptance_callback=*/base::DoNothing());
    return;
  }
  std::move(callback).Run(std::move(form), /*to_be_upserted_entries=*/{},
                          /*prompt_acceptance_callback=*/base::DoNothing());
}

void TestUserAnnotationsService::RetrieveAllEntries(
    base::OnceCallback<void(UserAnnotationsEntries)> callback) {
  count_entries_retrieved_++;
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

void TestUserAnnotationsService::GetCountOfValuesContainedBetween(
    base::Time,
    base::Time,
    base::OnceCallback<void(int)> callback) {
  std::move(callback).Run(entries_.size());
}
}  // namespace user_annotations
