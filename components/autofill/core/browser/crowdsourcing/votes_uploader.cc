// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/votes_uploader.h"

#include "base/containers/to_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_encoding.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"
#include "components/autofill/core/browser/crowdsourcing/determine_possible_field_types.h"
#include "components/autofill/core/browser/crowdsourcing/randomized_encoder.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/form_import/form_data_importer.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/field_filling_stats_and_score_metrics.h"
#include "components/autofill/core/browser/metrics/form_interactions_ukm_logger.h"
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

struct VotesUploader::PendingVote {
  LocalFrameToken frame_of_form;
  FormSignature form_signature;
  base::OnceClosure upload_vote;
};

VotesUploader::VotesUploader(AutofillClient* client) : client_(*client) {
  driver_observer_.Observe(&client_->GetAutofillDriverFactory());
}

VotesUploader::~VotesUploader() = default;

base::SequencedTaskRunner& VotesUploader::task_runner() {
  if (!task_runner_) {
    // If the priority is BEST_EFFORT, the task can be preempted, which is
    // thought to cause high memory usage (as memory is retained by the task
    // while it is preempted), https://crbug.com/974249
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  }
  return *task_runner_;
}

void VotesUploader::WipePendingVotesForForm(FormSignature form_signature) {
  std::erase_if(pending_votes_, [form_signature](const PendingVote& vote) {
    return vote.form_signature == form_signature;
  });
}

void VotesUploader::FlushPendingVotesForFrame(const LocalFrameToken& frame) {
  // Removes from `list` all elements that satisfy `pred` and returns them
  // in a separate list.
  auto extract_if = []<typename T>(std::list<T>& list, auto pred) {
    // stable_partition() returns the range of elements that *satisfy* `pred`.
    auto r = std::ranges::stable_partition(list, std::not_fn(pred));
    std::list<T> removed;
    // splice() moves the elements that satisfy `pred` to `removed`.
    removed.splice(removed.end(), list, r.begin(), r.end());
    return removed;
  };

  // We remove the callbacks from `pending_votes_` *before* invoking them.
  // The motivation is that we don't want to loop over `pending_votes_`
  // and call member functions in the loop's body for memory safety reasons.
  auto is_from_frame = [&](const PendingVote& vote) {
    return vote.frame_of_form == frame;
  };
  size_t num_old_queued_votes = pending_votes_.size();
  std::list<PendingVote> votes_of_frame =
      extract_if(pending_votes_, is_from_frame);
  DCHECK_EQ(pending_votes_.size() + votes_of_frame.size(),
            num_old_queued_votes);
  DCHECK(std::ranges::all_of(votes_of_frame, is_from_frame));
  DCHECK(std::ranges::none_of(pending_votes_, is_from_frame));
  for (PendingVote& vote : votes_of_frame) {
    std::move(vote.upload_vote).Run();
  }
}

void VotesUploader::OnAutofillDriverFactoryDestroyed(
    AutofillDriverFactory& factory) {
  driver_observer_.Reset();
}

// We want to flush votes in three cases:
// (1) The frame goes into BFcache.
// (2) The frame will be deleted. If there are no pending votes, this is the
//     time to flush. Otherwise, we want to wait for the pending votes and
//     then flush.
// (3) The frame *was* (not: will be) reset. The difference between "was" and
//     "will be" is important because BrowserAutofillManager::Reset() may add
//     new votes that should be flushed.
void VotesUploader::OnAutofillDriverStateChanged(
    AutofillDriverFactory& factory,
    AutofillDriver& driver,
    AutofillDriver::LifecycleState old_state,
    AutofillDriver::LifecycleState new_state) {
  // Calls FlushPendingVotes() after the currently pending
  // DeterminePossibleFieldTypesForUpload() / OnFieldTypesDetermined() tasks are
  // finished.
  auto delayed_flush_queued_votes_for_frame =
      [this](const LocalFrameToken& frame) {
        // Since task_runner() is a sequenced task runner, FlushPendingVotes()
        // will be called after any pending tasks and their replies.
        task_runner().PostTaskAndReply(
            FROM_HERE, base::DoNothing(),
            base::BindOnce(&VotesUploader::FlushPendingVotesForFrame,
                           weak_ptr_factory_.GetWeakPtr(), frame));
      };

  using enum AutofillDriver::LifecycleState;
  switch (new_state) {
    case kInactive:
      if (old_state == kActive) {
        // Case (1): The frame has become inactive (i.e., entered bfcache).
        delayed_flush_queued_votes_for_frame(driver.GetFrameToken());
      }
      break;
    case kActive:
      break;
    case kPendingReset:
      // Don't do anything yet. We wait for the transition *out* of the
      // kPendingReset state (i.e., `old_state == kPendingReset`) because new
      // votes may be cast in the kPendingReset state (specifically by
      // BrowserAutofillManager::Reset()).
      break;
    case kPendingDeletion:
      // Case (2): The frame will be deleted.
      delayed_flush_queued_votes_for_frame(driver.GetFrameToken());
      break;
  }

  if (old_state == kPendingReset) {
    // Case (3): The was reset.
    FlushPendingVotesForFrame(driver.GetFrameToken());
  }
}

bool VotesUploader::MaybeStartVoteUploadProcess(
    std::unique_ptr<FormStructure> form,
    bool observed_submission,
    LanguageCode current_page_language,
    base::TimeTicks initial_interaction_timestamp,
    const std::u16string& last_unlocked_credit_card_cvc,
    ukm::SourceId ukm_source_id) {
  // Only upload server statistics and UMA metrics if at least some local data
  // is available to use as a baseline.
  std::vector<const AutofillProfile*> profiles =
      client_->GetPersonalDataManager().address_data_manager().GetProfiles();
  if (observed_submission && form->IsAutofillable()) {
    AutofillMetrics::LogNumberOfProfilesAtAutofillableFormSubmission(
        profiles.size());
  }

  const std::vector<const CreditCard*>& credit_cards =
      client_->GetPersonalDataManager()
          .payments_data_manager()
          .GetCreditCards();

  base::span<const EntityInstance> entities;
  if (EntityDataManager* edm = client_->GetEntityDataManager()) {
    entities = edm->GetEntityInstances();
  }

  std::vector<LoyaltyCard> loyalty_cards;
  if (ValuablesDataManager* valuables_data_manager =
          client_->GetValuablesDataManager()) {
    loyalty_cards = base::ToVector(valuables_data_manager->GetLoyaltyCards(),
                                   [](LoyaltyCard card) { return card; });
  }

  if (profiles.empty() && credit_cards.empty() && entities.empty() &&
      loyalty_cards.empty()) {
    return false;
  }

  if (form->field_count() * (profiles.size() + credit_cards.size() +
                             entities.size() + loyalty_cards.size()) >=
      kMaxTypeMatchingCalls) {
    return false;
  }

  FormStructure::FormAssociations form_associations;
  if (form->IsAutofillable()) {
    form_associations = client_->GetFormDataImporter()->GetFormAssociations(
        form->form_signature());
  }

  // Annotate the form with the source language of the page.
  form->set_current_page_language(current_page_language);

  // Determine |ADDRESS_HOME_STATE| as a possible types for the fields in the
  // |form| with the help of |AlternativeStateNameMap|.
  // |AlternativeStateNameMap| can only be accessed on the main UI thread.
  std::set<FieldGlobalId> fields_that_match_state =
      PreProcessStateMatchingTypes(profiles, *form, client_->GetAppLocale());

  task_runner().PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const std::vector<AutofillProfile>& profiles,
             const std::vector<CreditCard>& credit_cards,
             const std::vector<EntityInstance>& entities,
             const std::vector<LoyaltyCard>& loyalty_cards,
             const std::u16string& last_unlocked_credit_card_cvc,
             const std::string& app_locale, bool observed_submission,
             std::unique_ptr<FormStructure> form,
             std::optional<RandomizedEncoder> randomized_encoder,
             FormStructure::FormAssociations form_associations,
             std::set<FieldGlobalId> fields_that_match_state) {
            DeterminePossibleFieldTypesForUpload(
                profiles, credit_cards, entities, loyalty_cards,
                fields_that_match_state, last_unlocked_credit_card_cvc,
                app_locale, *form);

            EncodeUploadRequestOptions options;
            options.encoder = std::move(randomized_encoder);
            options.form_associations = std::move(form_associations);
            options.observed_submission = observed_submission;
            options.available_field_types = DetermineAvailableFieldTypes(
                profiles, credit_cards, entities, loyalty_cards,
                last_unlocked_credit_card_cvc, app_locale);
            for (auto& [field_id, format_strings] :
                 DeterminePossibleFormatStringsForUpload(form->fields())) {
              options.fields[field_id].format_strings =
                  std::move(format_strings);
            }

            std::vector<AutofillUploadContents> upload_contents =
                EncodeUploadRequest(*form, options);
            return std::pair(std::move(form), std::move(upload_contents));
          },
          // Beware not to bind std::vector<T*> or base::span<T> because this
          // function is called asynchronously.
          base::ToVector(profiles, [](const auto* ptr) { return *ptr; }),
          base::ToVector(credit_cards, [](const auto* ptr) { return *ptr; }),
          base::ToVector(entities), std::move(loyalty_cards),
          last_unlocked_credit_card_cvc, client_->GetAppLocale(),
          observed_submission, std::move(form),
          RandomizedEncoder::Create(client_->GetPrefs()),
          std::move(form_associations), std::move(fields_that_match_state)),
      base::BindOnce(&VotesUploader::OnFieldTypesDetermined,
                     weak_ptr_factory_.GetWeakPtr(),
                     initial_interaction_timestamp, base::TimeTicks::Now(),
                     observed_submission, last_unlocked_credit_card_cvc,
                     ukm_source_id));
  return true;
}

void VotesUploader::OnFieldTypesDetermined(
    base::TimeTicks initial_interaction_timestamp,
    base::TimeTicks submission_timestamp,
    bool observed_submission,
    const std::u16string& last_unlocked_credit_card_cvc,
    ukm::SourceId ukm_source_id,
    std::pair<std::unique_ptr<FormStructure>,
              std::vector<AutofillUploadContents>> form_and_upload_contents) {
  auto& [form, upload_contents] = form_and_upload_contents;
  LocalFrameToken frame = form->global_id().frame_token;
  WipePendingVotesForForm(form->form_signature());
  if (observed_submission) {
    UploadVote(std::move(form), std::move(upload_contents),
               initial_interaction_timestamp, submission_timestamp,
               observed_submission, last_unlocked_credit_card_cvc,
               ukm_source_id);
    FlushPendingVotesForFrame(frame);
  } else {
    FlushOldestPendingVotesIfNecessary();
    pending_votes_.push_front(
        {.frame_of_form = frame,
         .form_signature = form->form_signature(),
         .upload_vote = base::BindOnce(
             &VotesUploader::UploadVote, weak_ptr_factory_.GetWeakPtr(),
             std::move(form), std::move(upload_contents),
             initial_interaction_timestamp, submission_timestamp,
             observed_submission, last_unlocked_credit_card_cvc,
             ukm_source_id)});
  }
}

void VotesUploader::FlushOldestPendingVotesIfNecessary() {
  // Entries in pending_votes_ are submitted after navigations or form
  // submissions. To reduce the risk of collecting too much data that is not
  // send, we allow only `kMaxEntriesInQueue` entries. Anything in excess will
  // be sent when the queue becomes to long.
  constexpr int kMaxEntriesInQueue = 10;
  while (pending_votes_.size() >= kMaxEntriesInQueue) {
    base::OnceCallback oldest_callback =
        std::move(pending_votes_.back().upload_vote);
    pending_votes_.pop_back();
    std::move(oldest_callback).Run();
  }
}

void VotesUploader::UploadVote(
    std::unique_ptr<FormStructure> submitted_form,
    std::vector<AutofillUploadContents> upload_contents,
    base::TimeTicks initial_interaction_timestamp,
    base::TimeTicks submission_timestamp,
    bool observed_submission,
    const std::u16string& last_unlocked_credit_card_cvc,
    ukm::SourceId ukm_source_id) {
  auto count_types = [&submitted_form](FormType type) {
    return std::ranges::count_if(
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
    client_->TriggerUserPerceptionOfAutofillSurvey(
        FillingProduct::kAddress,
        FormFillingStatsToSurveyStringData(address_filling_stats));
  } else if (can_trigger_credit_card_survey &&
             base::FeatureList::IsEnabled(
                 features::kAutofillCreditCardUserPerceptionSurvey)) {
    client_->TriggerUserPerceptionOfAutofillSurvey(
        FillingProduct::kCreditCard,
        FormFillingStatsToSurveyStringData(credit_card_filling_stats));
  }

  // If the form is submitted, we don't need to send pending votes from blur
  // (un-focus) events.
  if (submitted_form->ShouldRunHeuristics() ||
      submitted_form->ShouldRunHeuristicsForSingleFields() ||
      submitted_form->ShouldBeQueried()) {
    autofill_metrics::LogQualityMetrics(
        *submitted_form, submitted_form->form_parsed_timestamp(),
        initial_interaction_timestamp, submission_timestamp,
        client_->GetFormInteractionsUkmLogger(), ukm_source_id,
        observed_submission);
  }
  if (!submitted_form->ShouldBeUploaded()) {
    return;
  }
  if (autofill_metrics::ShouldRecordUkm() &&
      submitted_form->ShouldUploadUkm(
          /*require_classified_field=*/true)) {
    AutofillMetrics::LogAutofillFieldInfoAfterSubmission(
        client_->GetUkmRecorder(), ukm_source_id, *submitted_form,
        submission_timestamp);
  }
  client_->GetCrowdsourcingManager().StartUploadRequest(
      std::move(upload_contents), submitted_form->submission_source(),
      /*is_password_manager_upload=*/false);
}

}  // namespace autofill
