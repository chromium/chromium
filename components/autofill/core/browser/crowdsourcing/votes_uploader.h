// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_VOTES_UPLOADER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_VOTES_UPLOADER_H_

#include <list>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/common/language_code.h"
#include "components/autofill/core/common/signatures.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace autofill {

class BrowserAutofillManager;

// Determines, buffers, and uploads votes for a form to the crowdsourcing
// server.
//
// A form's vote contains the `FormSignature` and, for each field, a tuple of
// the `FieldSignature` and its `FieldType`, and further metadata. See
// autofill_crowdsourcing_encoding.h for further details.
//
// In VotesUploader, "to vote" also includes "to emit quality metrics".
// For brevity, function names don't mention the metrics explicitly.
//
// VotesUploader enqueues votes that are cast before form submission are
// enqueued. New votes for a form signature replace already-enqueued ones for
// that signature. Enqueued votes are flushed by BrowserAutofillManager when it
// dies or is reset.
//
//   MaybeStartVoteUploadProcess()◄─────────BrowserAutofillManager
//       │
//       │async
//       │
//       ▼
//   DeterminePossibleFieldTypesForUpload()
//       │
//       │       if submission
//       ├──────►────────────────────────────────┐
//       │else                                   │
//       │
//       ▼                                       │
//   QueueVote()                                 │
//   - Runs oldest callbacks if buffer is full.  │
//   - Stores a vote callback.                   │
//                                               │
//   QueuedVote::callback                        │
//       │                                       │
//       ├──────◄────────────────────────────────┘
//       │
//       ▼
//   OnSubmissionFieldTypesDetermined()
//       │
//       │
//       ▼
//   UploadVote()
//       │
//       │if submission
//       │
//       ▼
//   FlushPendingVotes()◄────────────────BrowserAutofillManager
//
// TODO(crbug.com/374086145): Investigate if vote flushing should be decoupled
// from BrowserAutofillManager lifetime.
//
// Owned by BrowserAutofillManager.
// TODO(crbug.com/374086145): Move ownership to AutofillClient.
class VotesUploader {
 public:
  explicit VotesUploader(BrowserAutofillManager* owner);
  VotesUploader(const VotesUploader&) = delete;
  VotesUploader& operator=(const VotesUploader&) = delete;
  virtual ~VotesUploader();

  // Will send an upload based on the |form_structure| data and the local
  // Autofill profile data. |observed_submission| is specified if the upload
  // follows an observed submission event. Returns false if the upload couldn't
  // start.
  virtual bool MaybeStartVoteUploadProcess(
      std::unique_ptr<FormStructure> form_structure,
      bool observed_submission,
      LanguageCode current_page_language,
      base::TimeTicks initial_interaction_timestamp,
      ukm::SourceId ukm_source_id);

  // Triggers and wipes all pending votes.
  void FlushPendingVotes();

  // TODO(crbug.com/374086145): Remove public member.
  std::u16string last_unlocked_credit_card_cvc_;

 protected:
  // Stores a `callback` for `form_signature`, possibly overriding an older
  // callback for `form_signature` or triggering a pending callback in case too
  // many callbacks are stored to create space.
  // Virtual and protected for testing.
  virtual void QueueVote(FormSignature form_signature,
                         base::OnceClosure callback);

  // Logs quality metrics for the |submitted_form| and uploads votes for the
  // field types to the crowdsourcing server, if appropriate.
  // |observed_submission| indicates whether the upload is a result of an
  // observed submission event.
  // Virtual and protected for testing.
  virtual void UploadVote(std::unique_ptr<FormStructure> submitted_form,
                          base::TimeTicks interaction_time,
                          base::TimeTicks submission_time,
                          bool observed_submission,
                          ukm::SourceId source_id);

 private:
  friend class VotesUploaderTestApi;

  struct QueuedVote;

  // Method called after the values present on submitted fields were associated
  // with Autofill field types. It is used to route calls to
  // `UploadVotesAndLogQuality()` and
  // `AutofillClient::TriggerUserPerceptionOfAutofillSurvey()`, since both
  // depend on the field types being determined.
  void OnSubmissionFieldTypesDetermined(
      std::unique_ptr<FormStructure> submitted_form,
      base::TimeTicks interaction_time,
      base::TimeTicks submission_time,
      bool observed_submission,
      ukm::SourceId source_id);

  // Removes a callback for the given `form_signature` without calling it.
  void WipePendingVotesForForm(FormSignature form_signature);

  AutofillClient& client();

  // List of callbacks to be called for sending blur votes. Only one callback is
  // stored per FormSignature. We rely on FormSignatures rather than
  // FormGlobalId to send votes for the various signatures of a form while it
  // evolves (when fields are added or removed). The list of callbacks is
  // ordered by time of creation: newest elements first. If the list becomes too
  // long, the oldest pending callbacks are just called and popped removed the
  // list.
  //
  // Callbacks are triggered in the following situations:
  // - We observe a form submission.
  // - The list becomes to large.

  // Callbacks are wiped in the following situations:
  // - A form is submitted.
  // - A callback is overridden by a more recent version.
  std::list<QueuedVote> queued_vote_uploads_;

  // This task runner sequentializes calls to
  // DeterminePossibleFieldTypesForUpload to ensure that blur votes are
  // processed before form submission votes. This is important so that a
  // submission can trigger the upload of blur votes.
  scoped_refptr<base::SequencedTaskRunner> vote_upload_task_runner_;

  // TODO(crbug.com/374086145): Remove or change to AutofillClient.
  raw_ref<BrowserAutofillManager> owner_;

  base::WeakPtrFactory<VotesUploader> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_VOTES_UPLOADER_H_
