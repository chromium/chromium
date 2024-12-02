// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/test_votes_uploader.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestVotesUploader::TestVotesUploader(BrowserAutofillManager* owner)
    : VotesUploader(owner) {}
TestVotesUploader::~TestVotesUploader() = default;

void TestVotesUploader::UploadVote(
    std::unique_ptr<FormStructure> submitted_form,
    base::TimeTicks interaction_time,
    base::TimeTicks submission_time,
    bool observed_submission,
    const ukm::SourceId source_id) {
  submitted_form_signature_ = submitted_form->FormSignatureAsStr();

  if (observed_submission) {
    // In case no submission was observed, the run_loop is quit in
    // StoreUploadVotesAndLogQualityCallback.
    run_loop_->Quit();
  }

  if (expected_observed_submission_ != std::nullopt) {
    EXPECT_EQ(expected_observed_submission_, observed_submission);
  }

  // If we have expected field types set, make sure they match.
  if (!expected_submitted_field_types_.empty()) {
    ASSERT_EQ(expected_submitted_field_types_.size(),
              submitted_form->field_count());
    for (size_t i = 0; i < expected_submitted_field_types_.size(); ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "Field %d with value %s", static_cast<int>(i),
          base::UTF16ToUTF8(
              submitted_form->field(i)->value(ValueSemantics::kCurrent))
              .c_str()));
      const FieldTypeSet& possible_types =
          submitted_form->field(i)->possible_types();
      EXPECT_EQ(expected_submitted_field_types_[i].size(),
                possible_types.size());
      for (auto it : expected_submitted_field_types_[i]) {
        EXPECT_TRUE(possible_types.count(it))
            << "Expected type: " << FieldTypeToStringView(it);
      }
    }
  }

  VotesUploader::UploadVote(std::move(submitted_form), interaction_time,
                            submission_time, observed_submission, source_id);
}

void TestVotesUploader::QueueVote(FormSignature form_signature,
                                  base::OnceClosure callback) {
  VotesUploader::QueueVote(form_signature, std::move(callback));
  run_loop_->Quit();
}

bool TestVotesUploader::MaybeStartVoteUploadProcess(
    std::unique_ptr<FormStructure> form_structure,
    bool observed_submission,
    LanguageCode current_page_language,
    base::TimeTicks initial_interaction_timestamp,
    ukm::SourceId ukm_source_id) {
  // The purpose of this runloop is to ensure that the field type determination
  // finishes. If `observed_submission` is true, it's terminated in
  // LogQualityAndUploadVotes. Otherwise, it is already terminated in
  // StoreUploadVotesAndLogQualityCallback.
  run_loop_ = std::make_unique<base::RunLoop>();
  if (VotesUploader::MaybeStartVoteUploadProcess(
          std::move(form_structure), observed_submission, current_page_language,
          initial_interaction_timestamp, ukm_source_id)) {
    run_loop_->Run();
    return true;
  }
  return false;
}

}  // namespace autofill
