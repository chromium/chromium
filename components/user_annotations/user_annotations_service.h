// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_SERVICE_H_
#define COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_SERVICE_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/user_annotations/user_annotations_types.h"
#include "url/gurl.h"

namespace autofill {
class FormStructure;
}  // namespace autofill

namespace optimization_guide {
class OptimizationGuideDecider;
namespace proto {
class AXTreeUpdate;
class UserAnnotationsEntry;
}  // namespace proto
}  // namespace optimization_guide

namespace os_crypt_async {
class Encryptor;
class OSCryptAsync;
}  // namespace os_crypt_async

namespace user_annotations {

class FormSubmissionHandler;
class UserAnnotationsDatabase;
struct Entry;

class UserAnnotationsService : public KeyedService {
 public:
  // `ImportFormCallback` carries `to_be_upserted_entries` that will be shown in
  // the Autofill prediction improvements prompt. The prompt then notifies the
  // `UserAnnotationsService` about the user decision by running
  // `prompt_acceptance_callback`, that is also provided by
  // `ImportFormCallback`. Note that `form` is always expected to be non-null.
  using ImportFormCallback = base::OnceCallback<void(
      std::unique_ptr<autofill::FormStructure> form,
      std::vector<optimization_guide::proto::UserAnnotationsEntry>
          to_be_upserted_entries,
      base::OnceCallback<void(bool prompt_was_accepted)>
          prompt_acceptance_callback)>;

  UserAnnotationsService(
      optimization_guide::OptimizationGuideModelExecutor* model_executor,
      const base::FilePath& storage_dir,
      os_crypt_async::OSCryptAsync* os_crypt_async,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);

  UserAnnotationsService(const UserAnnotationsService&) = delete;
  UserAnnotationsService& operator=(const UserAnnotationsService&) = delete;
  ~UserAnnotationsService() override;

  // Whether the form submission for `url` should be added to user annotations.
  // Virtual for testing.
  virtual bool ShouldAddFormSubmissionForURL(const GURL& url);

  // Adds a form submission to the user annotations. Calls `callback` according
  // to the outcome of the import process. The `callback` will notify Autofill
  // code about the import attempt so they can show a save prompt to the user.
  // When the prompt is closed, the inner `prompt_acceptance_callback` will
  // notify `this` about the user's decision. `form` is assumed to never be
  // `nullptr`. Virtual for testing.
  virtual void AddFormSubmission(
      const GURL& url,
      const std::string& title,
      optimization_guide::proto::AXTreeUpdate ax_tree_update,
      std::unique_ptr<autofill::FormStructure> form,
      ImportFormCallback callback);

  // Retrieves all entries from the database. Invokes `callback` when complete.
  // Virtual for testing.
  virtual void RetrieveAllEntries(
      base::OnceCallback<void(UserAnnotationsEntries)> callback);

  // Remove the user annotation entry with `entry_id` and calls `callback` upon
  // completion.
  // Virtual for testing.
  virtual void RemoveEntry(EntryID entry_id, base::OnceClosure callback);

  // Removes all the user annotation entries and calls `callback` upon
  // completion.
  // Virtual for testing.
  virtual void RemoveAllEntries(base::OnceClosure callback);

  // Removes the user annotation entries that were last modified from
  // `delete_begin` to `delete_end`.
  // Virtual for testing.
  virtual void RemoveAnnotationsInRange(const base::Time& delete_begin,
                                        const base::Time& delete_end);

  // Returns the number of unique user annotations that were last modified
  // between [`begin`, `end`).
  // Virtual for testing.
  virtual void GetCountOfValuesContainedBetween(
      base::Time begin,
      base::Time end,
      base::OnceCallback<void(int)> callback);

  // KeyedService:
  void Shutdown() override;

 private:
  friend class TestUserAnnotationsService;
  friend class FormSubmissionHandler;

  // Used in testing, to construct the service without encryptor and database.
  UserAnnotationsService();

  // Called when the encryptor is ready.
  void OnOsCryptAsyncReady(const base::FilePath& storage_dir,
                           os_crypt_async::Encryptor encryptor,
                           bool success);

  // Returns whether the database initialization is complete.
  bool IsDatabaseReady();

  // Starts the processing of next pending form submission.
  void ProcessNextFormSubmission();

  // Saves the entries to database.
  void SaveEntries(
      const optimization_guide::proto::FormsAnnotationsResponse& entries);

  // Called when the form submission is fully complete.
  void OnFormSubmissionComplete();

  optimization_guide::OptimizationGuideModelExecutor* model_executor() {
    return model_executor_;
  }

  // An in-memory representation of the "database" of user annotation entries.
  // Used only when `ShouldPersistUserAnnotations()` is false.
  std::vector<Entry> entries_;

  int64_t entry_id_counter_ = 0;

  // Database used to persist the user annotation entries.
  // Used only when `ShouldPersistUserAnnotations()` is true.
  base::SequenceBound<UserAnnotationsDatabase> user_annotations_database_;

  // Maintains the subscription for `OSCryptAsync` and cancels upon destruction.
  base::CallbackListSubscription encryptor_ready_subscription_;

  // The model executor to use to normalize entries. Guaranteed to outlive
  // `this`.
  raw_ptr<optimization_guide::OptimizationGuideModelExecutor> model_executor_;

  // The optimization guide decider to determine whether to generate user
  // annotations for a page. Guaranteed to outlive `this`.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_;

  // The override list for allowed hosts for forms annotations.
  // TODO: b/361692317 - Remove this once optimization guide actually populates
  // list.
  const std::vector<std::string> allowed_hosts_for_forms_annotations_;

  // The queue of form submissions pending to be processed. Each form submission
  // goes through multiple stages of processing, and until then the form
  // submissions will wait in this queue. The first entry is the one that is
  // currently being processed.
  base::queue<std::unique_ptr<FormSubmissionHandler>> pending_form_submissions_;

  base::WeakPtrFactory<UserAnnotationsService> weak_ptr_factory_{this};
};

}  // namespace user_annotations

#endif  // COMPONENTS_USER_ANNOTATIONS_USER_ANNOTATIONS_SERVICE_H_
