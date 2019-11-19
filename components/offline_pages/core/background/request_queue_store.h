// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_QUEUE_STORE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_QUEUE_STORE_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/background/request_queue.h"
#include "components/offline_pages/core/background/request_queue_store.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_store_types.h"

namespace base {
class SequencedTaskRunner;
}

namespace sql {
class Database;
}

namespace offline_pages {

// Persistent storage for the RequestQueue.
//
// This store has a history of schema updates.
// Original schema was delivered in M57. Since then the following changes
// happened:
// * In M58 original_url was added.
// * In M61 request_origin was added.
// * In M67 fail_state was added.
//
// TODO(romax): remove all activation_time related code the next we change the
// schema.
//
// Looking for procedure to update the schema, please refer to
// offline_page_metadata_store.h
class RequestQueueStore {
 public:
  enum class UpdateStatus {
    ADDED,    // Request was added successfully.
    UPDATED,  // Request was updated successfully.
    FAILED,   // Add or update attempt failed.
  };

  typedef base::OnceCallback<void(bool /* success */)> InitializeCallback;
  typedef base::OnceCallback<void(bool /* success */)> ResetCallback;
  typedef base::OnceCallback<void(
      bool /* success */,
      std::vector<std::unique_ptr<SavePageRequest>> /* requests */)>
      GetRequestsCallback;
  typedef base::OnceCallback<void(AddRequestResult)> AddCallback;
  using UpdateCallback = RequestQueue::UpdateCallback;
  using AddOptions = RequestQueue::AddOptions;

  RequestQueueStore(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const base::FilePath& database_dir);

  virtual ~RequestQueueStore();

  // Initializes the store. Should be called before any other methods.
  // Virtual for testing only.
  virtual void Initialize(InitializeCallback callback);

  // Gets all of the requests from the store.
  void GetRequests(GetRequestsCallback callback);

  // Gets requests with specified IDs from the store. UpdateCallback is used
  // instead of GetRequestsCallback to indicate which requests where not found.
  // Note: current implementation of this method makes a SQL query per ID. This
  // is OK as long as number of IDs stays low, which is a typical case.
  // Implementation should be revisited in case that presumption changes.
  void GetRequestsByIds(const std::vector<int64_t>& request_ids,
                        UpdateCallback callback);

  // Asynchronously adds request in store. Fails if request with the same
  // offline ID already exists.
  void AddRequest(const SavePageRequest& offline_page,
                  AddOptions options,
                  AddCallback callback);

  // Asynchronously updates requests in store.
  void UpdateRequests(const std::vector<SavePageRequest>& requests,
                      UpdateCallback callback);

  // Asynchronously removes requests from the store using their IDs.
  // Result of the update, and a number of removed pages is passed in the
  // callback.
  // Result of remove should be false, when one of the provided items couldn't
  // be deleted, e.g. because it was missing.
  void RemoveRequests(const std::vector<int64_t>& request_ids,
                      UpdateCallback callback);

  // Invokes |remove_predicate| for all requests in the queue, and removes each
  // request where |remove_predicate| returns true. Note: |remove_predicate| is
  // called from a background thread.
  void RemoveRequestsIf(const base::RepeatingCallback<
                            bool(const SavePageRequest&)>& remove_predicate,
                        UpdateCallback done_callback);

  // Asynchronously sets the auto fetch notification state on the request with
  // |request_id|.
  void SetAutoFetchNotificationState(
      int64_t request_id,
      SavePageRequest::AutoFetchNotificationState state,
      base::OnceCallback<void(bool updated)> callback);

  // Resets the store (removes any existing data).
  // Virtual for testing only.
  virtual void Reset(ResetCallback callback);

  // Gets the store state.
  StoreState state() const;

 protected:
  // Constructs an in-memory store. Used for testing only.
  explicit RequestQueueStore(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Helper function used to force incorrect state for testing purposes.
  void SetStateForTesting(StoreState state, bool reset_db);

 private:
  RequestQueueStore& operator=(const RequestQueueStore&) = delete;

  // Used to finalize DB connection initialization.
  void OnOpenConnectionDone(InitializeCallback callback, bool success);

  // Used to finalize DB connection reset.
  void OnResetDone(ResetCallback callback, bool success);

  // Helper function to return immediately if no database is found.
  bool CheckDb() const;

  // Background thread where all SQL access should be run.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Path to the database on disk.
  base::FilePath db_file_path_;

  // Database connection.
  std::unique_ptr<sql::Database> db_;

  // State of the store.
  StoreState state_;

  base::WeakPtrFactory<RequestQueueStore> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_BACKGROUND_REQUEST_QUEUE_STORE_H_
