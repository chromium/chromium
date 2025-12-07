// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_VOTES_UPLOADER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_VOTES_UPLOADER_H_

#include <list>
#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_driver_factory.h"
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
// In VotesUploader, "to vote" also includes "to emit quality metrics" and
// "to potentially display an Autofill survey". For brevity, function names
// don't mention this explicitly.
//
// VotesUploader enqueues votes that are cast before form submission are
// enqueued. New votes for a form signature replace already-enqueued ones for
// that signature. Enqueued votes are flushed by BrowserAutofillManager when it
// dies or is reset. Therefore, votes are called "pending" while they are in the
// queue.
//
//   MaybeStartVoteUploadProcess()◄─────────BrowserAutofillManager
//       │
//       │async
//       │
//       ▼
//   DeterminePossibleFieldTypesForUpload()
//       │
//       │async
//       │
//       ▼
//   OnFieldTypesDetermined()
//       │
//       │       if submission
//       ├──────►────────────────────────────────┐
//       │else                                   │
//       │                                       │
//       ▼                                       │
//   Store PendingVote, which is uploaded when   │
//   - a submission happens in the frame;        │
//   - the frame becomes inactive;               │
//   - the frame is reset;                       │
//   - the frame is deleted;                     │
//   - the queue becomes too large.              │
//                                               │
//   PendingVote::upload_vote                    │
//       │                                       │
//       │                                       │
//       ▼                                       │
//   UploadVote()◄───────────────────────────────┘
//
// Owned by AutofillClient. This is so votes can be determined and uploaded when
// the frame's AutofillDriver and AutofillManager have been destroyed already.
// Since the AutofillCrowdsourcingManager is also owned by AutofillClient and
// since uploading votes is asynchronous, this implies that voting cannot happen
// when the tab is closed.
class VotesUploader : public AutofillDriverFactory::Observer {
 public:
  explicit VotesUploader(AutofillClient* owner);
  VotesUploader(const VotesUploader&) = delete;
  VotesUploader& operator=(const VotesUploader&) = delete;
  ~VotesUploader() override;

  // Will send an upload based on the |form| data and the local Autofill profile
  // data. |observed_submission| is specified if the upload follows an observed
  // submission event. Returns false if the upload couldn't start.
  virtual bool MaybeStartVoteUploadProcess(
      std::unique_ptr<FormStructure> form,
      bool observed_submission,
      LanguageCode current_page_language,
      base::TimeTicks initial_interaction_timestamp,
      const std::u16string& last_unlocked_credit_card_cvc,
      ukm::SourceId ukm_source_id);

 protected:
  // Logs quality metrics, perhaps displays an Autofill survey, and uploads the
  // vote. All these three activities depend on the determined field types.
  //
  // `initial_interaction_timestamp` is the last interaction with the form
  // passed to MaybeStartVoteUploadProcess(). `submission_timestamp` is the time
  // MaybeStartVoteUploadProcess() was called. `observed_submission` indicates
  // whether the upload is a result of an observed submission event.
  // `ukm_source_id` is the form's page's UKM source ID.
  //
  // Virtual and protected for testing.
  virtual void UploadVote(std::unique_ptr<FormStructure> submitted_form,
                          std::vector<AutofillUploadContents> upload_contents,
                          base::TimeTicks initial_interaction_timestamp,
                          base::TimeTicks submission_timestamp,
                          bool observed_submission,
                          ukm::SourceId ukm_source_id);

 private:
  friend class VotesUploaderTestApi;

  struct PendingVote;

  // The reply of DeterminePossibleFieldTypesForUpload().
  // Either calls UploadVote() or stores a PendingVote.
  void OnFieldTypesDetermined(
      base::TimeTicks initial_interaction_timestamp,
      base::TimeTicks submission_timestamp,
      bool observed_submission,
      ukm::SourceId ukm_source_id,
      std::pair<std::unique_ptr<FormStructure>,
                std::vector<AutofillUploadContents>> form_and_upload_contents);

  // Uploads all pending votes for forms from `frame`.
  void FlushPendingVotesForFrame(const LocalFrameToken& frame);

  // Removes the callbacks for the given `form_signature` without calling them.
  void WipePendingVotesForForm(FormSignature form_signature);

  // Uploads the oldest votes if the queue of votes has become too long.
  void FlushOldestPendingVotesIfNecessary();

  // AutofillDriverFactory::Observer:
  void OnAutofillDriverFactoryDestroyed(
      AutofillDriverFactory& factory) override;
  void OnAutofillDriverStateChanged(
      AutofillDriverFactory& factory,
      AutofillDriver& driver,
      AutofillDriver::LifecycleState old_state,
      AutofillDriver::LifecycleState new_state) override;

  // Task runner for asynchronously determining possible field types.
  base::SequencedTaskRunner& task_runner();

  const raw_ref<AutofillClient> client_;

  // List of pending votes. These votes were cast not when a form submission
  // happened but, for example, when a form lost focus ("blur votes").
  //
  // Only one callback is stored per FormSignature. We rely on FormSignatures
  // rather than FormGlobalId to send votes for the various signatures of a form
  // while it evolves (when fields are added or removed). The list of votes is
  // ordered by time of creation: newest elements first. If the list becomes too
  // long, the oldest votes are popped from the list and uploaded.
  //
  // Beware that this (like the other members) must not be accessed from
  // `task_runner_`.
  std::list<PendingVote> pending_votes_;

  // Task runner for DeterminePossibleFieldTypesForUpload(). It is important
  // that this is a *sequenced* task runner for reasons:
  // - Form submission not only uploads the submission vote but also flushes
  //   blur votes. For that to work, the blur votes' OnFieldTypesDetermined()
  //   must be called before the submission vote's OnFieldTypesDetermined().
  // - When a frame becomes inactive or is reset, pending votes should be
  //   flushed -- but first, pending DeterminePossibleFieldTypesForUpload()
  //   calls need to finish.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::ScopedObservation<AutofillDriverFactory,
                          AutofillDriverFactory::Observer>
      driver_observer_{this};

  base::WeakPtrFactory<VotesUploader> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_VOTES_UPLOADER_H_
