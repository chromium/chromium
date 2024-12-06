// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/votes_uploader.h"

#include "base/containers/to_vector.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/crowdsourcing/determine_possible_field_types.h"
#include "components/autofill/core/browser/crowdsourcing/randomized_encoder.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"
#include "components/autofill/core/browser/metrics/quality_metrics.h"

namespace autofill {

namespace {

// The minimum required number of fields for an user perception survey to be
// triggered. This makes sure that for example forms that only contain a single
// email field do not prompt a survey. Such survey answer would likely taint
// our analysis.
constexpr size_t kMinNumberAddressFieldsToTriggerAddressUserPerceptionSurvey =
    4;

// Converts `filling_stats` to a key-value representation, where the key
// is the "stats category" and the value is the number of fields that match
// such category. This is used to show users a survey that will measure the
// perception of Autofill.
std::map<std::string, std::string> FormFillingStatsToSurveyStringData(
    autofill_metrics::FormGroupFillingStats& filling_stats) {
  return {
      {"Accepted fields", base::NumberToString(filling_stats.num_accepted)},
      {"Corrected to same type",
       base::NumberToString(filling_stats.num_corrected_to_same_type)},
      {"Corrected to a different type",
       base::NumberToString(filling_stats.num_corrected_to_different_type)},
      {"Corrected to an unknown type",
       base::NumberToString(filling_stats.num_corrected_to_unknown_type)},
      {"Corrected to empty",
       base::NumberToString(filling_stats.num_corrected_to_empty)},
      {"Manually filled to same type",
       base::NumberToString(filling_stats.num_manually_filled_to_same_type)},
      {"Manually filled to a different type",
       base::NumberToString(
           filling_stats.num_manually_filled_to_different_type)},
      {"Manually filled to an unknown type",
       base::NumberToString(filling_stats.num_manually_filled_to_unknown_type)},
      {"Total corrected", base::NumberToString(filling_stats.TotalCorrected())},
      {"Total filled", base::NumberToString(filling_stats.TotalFilled())},
      {"Total unfilled", base::NumberToString(filling_stats.TotalUnfilled())},
      {"Total manually filled",
       base::NumberToString(filling_stats.TotalManuallyFilled())},
      {"Total number of fields", base::NumberToString(filling_stats.Total())}};
}

}  // namespace

struct VotesUploader::QueuedVote {
  FormSignature form_signature;
  base::OnceClosure upload_vote;
};

VotesUploader::VotesUploader(BrowserAutofillManager* owner) : owner_(*owner) {}
VotesUploader::~VotesUploader() = default;

AutofillClient& VotesUploader::client() {
  return owner_->client();
}

base::SequencedTaskRunner& VotesUploader::vote_upload_task_runner() {
  if (!vote_upload_task_runner_) {
    // If the priority is BEST_EFFORT, the task can be preempted, which is
    // thought to cause high memory usage (as memory is retained by the task
    // while it is preempted), https://crbug.com/974249
    vote_upload_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  }
  return *vote_upload_task_runner_;
}

void VotesUploader::WipeQueuedVotesForForm(FormSignature form_signature) {
  std::erase_if(queued_votes_, [form_signature](const QueuedVote& vote) {
    return vote.form_signature == form_signature;
  });
}

void VotesUploader::FlushQueuedVotes() {
  std::list<QueuedVote> queued_vote_uploads = std::exchange(queued_votes_, {});
  for (QueuedVote& vote : queued_vote_uploads) {
    std::move(vote.upload_vote).Run();
  }
}

bool VotesUploader::MaybeStartVoteUploadProcess(
    std::unique_ptr<FormStructure> form,
    bool observed_submission,
    LanguageCode current_page_language,
    base::TimeTicks initial_interaction_timestamp,
    ukm::SourceId ukm_source_id) {
  // Only upload server statistics and UMA metrics if at least some local data
  // is available to use as a baseline.
  std::vector<const AutofillProfile*> profiles =
      client().GetPersonalDataManager().address_data_manager().GetProfiles();
  if (observed_submission && form->IsAutofillable()) {
    AutofillMetrics::LogNumberOfProfilesAtAutofillableFormSubmission(
        profiles.size());
  }

  const std::vector<CreditCard*>& credit_cards = client()
                                                     .GetPersonalDataManager()
                                                     .payments_data_manager()
                                                     .GetCreditCards();

  if (profiles.empty() && credit_cards.empty()) {
    return false;
  }

  if (form->field_count() * (profiles.size() + credit_cards.size()) >=
      kMaxTypeMatchingCalls) {
    return false;
  }

  // Copy the profile and credit card data, so that it can be accessed on a
  // separate thread.
  std::vector<AutofillProfile> copied_profiles = base::ToVector(
      profiles, [](const AutofillProfile* profile) { return *profile; });
  std::vector<CreditCard> copied_credit_cards = base::ToVector(
      credit_cards, [](const CreditCard* card) { return *card; });

  // Annotate the form with the source language of the page.
  form->set_current_page_language(current_page_language);

  // Attach the Randomized Encoder.
  form->set_randomized_encoder(RandomizedEncoder::Create(client().GetPrefs()));

  // Determine |ADDRESS_HOME_STATE| as a possible types for the fields in the
  // |form| with the help of |AlternativeStateNameMap|.
  // |AlternativeStateNameMap| can only be accessed on the main UI thread.
  PreProcessStateMatchingTypes(client(), copied_profiles, *form);

  // TODO(crbug.com/368306576): Bound the size of `copied_profiles` and
  // `copied_credit_cards` by `kMaxDataConsideredForPossibleTypes` and make
  // the call to DeterminePossibleFieldTypesForUpload() synchronous.
  vote_upload_task_runner().PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const std::vector<AutofillProfile>& profiles,
             const std::vector<CreditCard>& credit_cards,
             const std::u16string& last_unlocked_credit_card_cvc,
             const std::string& app_locale,
             std::unique_ptr<FormStructure> form) {
            DeterminePossibleFieldTypesForUpload(profiles, credit_cards,
                                                 last_unlocked_credit_card_cvc,
                                                 app_locale, form.get());
            return form;
          },
          std::move(copied_profiles), std::move(copied_credit_cards),
          last_unlocked_credit_card_cvc_, client().GetAppLocale(),
          std::move(form)),
      base::BindOnce(&VotesUploader::OnFieldTypesDetermined,
                     weak_ptr_factory_.GetWeakPtr(),
                     initial_interaction_timestamp, base::TimeTicks::Now(),
                     observed_submission, ukm_source_id));
  return true;
}

void VotesUploader::OnFieldTypesDetermined(
    base::TimeTicks initial_interaction_timestamp,
    base::TimeTicks submission_timestamp,
    bool observed_submission,
    ukm::SourceId ukm_source_id,
    std::unique_ptr<FormStructure> form) {
  if (observed_submission) {
    UploadVote(std::move(form), initial_interaction_timestamp,
               submission_timestamp, observed_submission, ukm_source_id);
  } else {
    WipeQueuedVotesForForm(form->form_signature());
    TruncateQueueIfNecessary();
    queued_votes_.push_front(
        {.form_signature = form->form_signature(),
         .upload_vote = base::BindOnce(
             &VotesUploader::UploadVote, weak_ptr_factory_.GetWeakPtr(),
             std::move(form), initial_interaction_timestamp,
             submission_timestamp, observed_submission, ukm_source_id)});
  }
}

void VotesUploader::TruncateQueueIfNecessary() {
  // Entries in queued_votes_ are submitted after navigations or form
  // submissions. To reduce the risk of collecting too much data that is not
  // send, we allow only `kMaxEntriesInQueue` entries. Anything in excess will
  // be sent when the queue becomes to long.
  constexpr int kMaxEntriesInQueue = 10;
  while (queued_votes_.size() >= kMaxEntriesInQueue) {
    base::OnceCallback oldest_callback =
        std::move(queued_votes_.back().upload_vote);
    queued_votes_.pop_back();
    std::move(oldest_callback).Run();
  }
}

void VotesUploader::UploadVote(std::unique_ptr<FormStructure> submitted_form,
                               base::TimeTicks initial_interaction_timestamp,
                               base::TimeTicks submission_timestamp,
                               bool observed_submission,
                               ukm::SourceId ukm_source_id) {
  auto count_types = [&submitted_form](FormType type) {
    return base::ranges::count_if(
        submitted_form->fields(),
        [=](const std::unique_ptr<AutofillField>& field) {
          return FieldTypeGroupToFormType(field->Type().group()) == type;
        });
  };

  size_t address_fields_count = count_types(FormType::kAddressForm);
  autofill_metrics::FormGroupFillingStats address_filling_stats =
      autofill_metrics::GetFormFillingStatsForFormType(FormType::kAddressForm,
                                                       *submitted_form);
  const bool can_trigger_address_survey =
      address_fields_count >=
          kMinNumberAddressFieldsToTriggerAddressUserPerceptionSurvey &&
      address_filling_stats.TotalFilled() > 0 &&
      base::FeatureList::IsEnabled(
          features::kAutofillAddressUserPerceptionSurvey);

  size_t credit_card_fields_count = count_types(FormType::kCreditCardForm);
  autofill_metrics::FormGroupFillingStats credit_card_filling_stats =
      autofill_metrics::GetFormFillingStatsForFormType(
          FormType::kCreditCardForm, *submitted_form);
  const bool can_trigger_credit_card_survey =
      credit_card_fields_count > 0 &&
      credit_card_filling_stats.TotalFilled() > 0;

  if (can_trigger_address_survey) {
    client().TriggerUserPerceptionOfAutofillSurvey(
        FillingProduct::kAddress,
        FormFillingStatsToSurveyStringData(address_filling_stats));
  } else if (can_trigger_credit_card_survey &&
             base::FeatureList::IsEnabled(
                 features::kAutofillCreditCardUserPerceptionSurvey)) {
    client().TriggerUserPerceptionOfAutofillSurvey(
        FillingProduct::kCreditCard,
        FormFillingStatsToSurveyStringData(credit_card_filling_stats));
  }

  // If the form is submitted, we don't need to send pending votes from blur
  // (un-focus) events.
  if (observed_submission) {
    WipeQueuedVotesForForm(submitted_form->form_signature());
  }
  if (submitted_form->ShouldRunHeuristics() ||
      submitted_form->ShouldRunHeuristicsForSingleFields() ||
      submitted_form->ShouldBeQueried()) {
    // TODO(crbug.com/374086145): Eliminate reference to `owner_`.
    autofill_metrics::LogQualityMetrics(
        *submitted_form, submitted_form->form_parsed_timestamp(),
        initial_interaction_timestamp, submission_timestamp,
        owner_->client().GetFormInteractionsUkmLogger(), ukm_source_id,
        observed_submission);
    if (observed_submission) {
      // Ensure that callbacks for blur votes get sent as well here because
      // we are not sure whether a full navigation with a Reset() call follows.
      FlushQueuedVotes();
    }
  }
  if (!submitted_form->ShouldBeUploaded()) {
    return;
  }
  if (autofill_metrics::ShouldRecordUkm() &&
      submitted_form->ShouldUploadUkm(
          /*require_classified_field=*/true)) {
    AutofillMetrics::LogAutofillFieldInfoAfterSubmission(
        client().GetUkmRecorder(), ukm_source_id, *submitted_form,
        submission_timestamp);
  }
  const PersonalDataManager& pdm = client().GetPersonalDataManager();
  FieldTypeSet non_empty_types;
  for (const AutofillProfile* profile :
       pdm.address_data_manager().GetProfiles()) {
    profile->GetNonEmptyTypes(client().GetAppLocale(), &non_empty_types);
  }
  for (const CreditCard* card : pdm.payments_data_manager().GetCreditCards()) {
    card->GetNonEmptyTypes(client().GetAppLocale(), &non_empty_types);
  }
  // As CVC is not stored, treat it separately.
  if (!last_unlocked_credit_card_cvc_.empty() ||
      non_empty_types.contains(CREDIT_CARD_NUMBER)) {
    non_empty_types.insert(CREDIT_CARD_VERIFICATION_CODE);
  }
  client().GetCrowdsourcingManager().StartUploadRequest(
      /*upload_contents=*/EncodeUploadRequest(*submitted_form, non_empty_types,
                                              /*login_form_signature=*/{},
                                              observed_submission),
      submitted_form->submission_source(),
      /*is_password_manager_upload=*/false);
}

}  // namespace autofill
