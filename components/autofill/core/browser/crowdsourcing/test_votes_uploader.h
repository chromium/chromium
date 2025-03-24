// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_TEST_VOTES_UPLOADER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_TEST_VOTES_UPLOADER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/crowdsourcing/votes_uploader.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/common/language_code.h"
#include "components/autofill/core/common/signatures.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace autofill {

// Turns the asynchronous VotesUploader operations into synchronous ones and
// validates expectations.
class TestVotesUploader : public VotesUploader {
 public:
  explicit TestVotesUploader(AutofillClient* client);
  TestVotesUploader(const TestVotesUploader&) = delete;
  TestVotesUploader& operator=(const TestVotesUploader&) = delete;
  ~TestVotesUploader() override;

  // Runs VotesUploader::MaybeStartVoteUploadProcess() synchronously.
  bool MaybeStartVoteUploadProcess(
      std::unique_ptr<FormStructure> form_structure,
      bool observed_submission,
      LanguageCode current_page_language,
      base::TimeTicks initial_interaction_timestamp,
      const std::u16string& last_unlocked_credit_card_cvc,
      ukm::SourceId ukm_source_id) override;

  void UploadVote(std::unique_ptr<FormStructure> submitted_form,
                  std::vector<AutofillUploadContents> upload_contents,
                  base::TimeTicks initial_interaction_timestamp,
                  base::TimeTicks submission_timestamp,
                  bool observed_submission,
                  const std::u16string& last_unlocked_credit_card_cvc,
                  const ukm::SourceId source_id) override;

  const std::string& submitted_form_signature() {
    return submitted_form_signature_;
  }

  void set_expected_submitted_field_types(
      std::vector<FieldTypeSet> expected_types) {
    expected_submitted_field_types_ = std::move(expected_types);
  }

  void set_expected_observed_submission(bool expected) {
    expected_observed_submission_ = expected;
  }

  const FormStructure::FormAssociations& get_last_uploaded_form_associations()
      const {
    return last_uploaded_form_associations_;
  }

 private:
  friend class TestBrowserAutofillManager;

  FormStructure::FormAssociations last_uploaded_form_associations_;
  std::string submitted_form_signature_;
  std::optional<bool> expected_observed_submission_;
  std::vector<FieldTypeSet> expected_submitted_field_types_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_TEST_VOTES_UPLOADER_H_
