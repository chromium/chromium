// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_BROWSER_DM_TOKEN_STORAGE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_BROWSER_DM_TOKEN_STORAGE_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/system/sys_info.h"
#include "base/task/single_thread_task_runner.h"
#include "components/policy/core/common/cloud/dm_token.h"

namespace base {
class TaskRunner;
}

namespace policy {

// Manages storing and retrieving tokens and client ID used to enroll browser
// instances for enterprise management. The tokens are read from disk or
// registry once and cached values are returned in subsequent calls.
//
// All calls to member functions must be sequenced. It is an error to attempt
// concurrent store operations. RetrieveClientId must be the first method
// called.
class BrowserDMTokenStorage {
 public:
  using StoreTask = base::OnceCallback<bool()>;
  using StoreCallback = base::OnceCallback<void(bool success)>;

  // Delegate pattern for platform-dependant operations.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Gets the client ID and returns it.
    virtual std::string InitClientId() = 0;
    // Gets the enrollment token and returns it.
    virtual std::string InitEnrollmentToken() = 0;
    // Gets the DM token and returns it.
    virtual std::string InitDMToken() = 0;
    // Gets the boolean value that determines if error message will be
    // displayed when enrollment fails.
    virtual bool InitEnrollmentErrorOption() = 0;
    // Returns whether the enrollment token can be initialized (if it is not
    // already) when `InitIfNeeded` is called.
    virtual bool CanInitEnrollmentToken() const = 0;
    // Function called by `SaveDMToken()` that returns if the operation was a
    // success.
    virtual StoreTask SaveDMTokenTask(const std::string& token,
                                      const std::string& client_id) = 0;
    // Function called by `DeleteDMToken()` that returns if the operation was a
    // success.
    virtual StoreTask DeleteDMTokenTask(const std::string& client_id) = 0;
    // Gets the specific task runner that should be used by |SaveDMToken|.
    virtual scoped_refptr<base::TaskRunner> SaveDMTokenTaskRunner() = 0;
  };

  // Returns the global singleton object. Must be called from the UI thread. The
  // first caller must set the platform-specific delegate via SetDelegate().
  static BrowserDMTokenStorage* Get();

  BrowserDMTokenStorage(const BrowserDMTokenStorage&) = delete;
  BrowserDMTokenStorage& operator=(const BrowserDMTokenStorage&) = delete;

  // Sets the delegate to use for platform-specific operations.
  static void SetDelegate(std::unique_ptr<Delegate> delegate);

  // Returns a client ID unique to the machine.
  std::string RetrieveClientId();
  // Returns the enrollment token, or an empty string if there is none.
  std::string RetrieveEnrollmentToken();
  // Asynchronously stores |dm_token| and calls |callback| with a boolean to
  // indicate success or failure. It is an error to attempt concurrent store
  // operations.
  void StoreDMToken(const std::string& dm_token, StoreCallback callback);
  // Asynchronously invalidates |dm_token_| and calls |callback| with a boolean
  // to indicate success or failure. It is an error to attempt concurrent store
  // operations.
  void InvalidateDMToken(StoreCallback callback);
  // Asynchronously clears |dm_token_| and calls |callback| with a boolean to
  // indicate success or failure. It is an error to attempt concurrent store
  // operations.
  void ClearDMToken(StoreCallback callback);
  // Returns an already stored DM token. An empty token is returned if no DM
  // token exists on the system or an error is encountered.
  DMToken RetrieveDMToken();
  // Must be called after the DM token is saved, to ensure that the callback is
  // invoked.
  void OnDMTokenStored(bool success);

  // Return true if we display error message dialog when enrollment process
  // fails.
  virtual bool ShouldDisplayErrorMessageOnFailure();

  // Set the BrowserDMTokenStorage instance for testing. The caller owns the
  // instance of the storage.
  static void SetForTesting(BrowserDMTokenStorage* storage) {
    storage_for_testing_ = storage;
  }
  // Force the class to initialize again. Use it when some fields are changed
  // during test.
  void ResetForTesting() { is_initialized_ = false; }

 protected:
  friend class base::NoDestructor<BrowserDMTokenStorage>;

  // The platform-specific delegate. Must be set via
  // BrowserDMTokenStorage::SetDelegate() before other methods can be called.
  std::unique_ptr<Delegate> delegate_;

  // Get the global singleton instance by calling BrowserDMTokenStorage::Get().
  BrowserDMTokenStorage();
  virtual ~BrowserDMTokenStorage();

 private:
  static BrowserDMTokenStorage* storage_for_testing_;

  // Initializes the DMTokenStorage object and caches the ids and tokens. This
  // is called the first time the BrowserDMTokenStorage is interacted with.
  void InitIfNeeded();

  // Saves the DM token.
  void SaveDMToken(const std::string& token);
  // Deletes the DM token.
  void DeleteDMToken();

  // Will be called after the DM token is stored.
  StoreCallback store_callback_;

  bool is_initialized_{false};
  bool is_init_enrollment_token_skipped_{true};

  std::string client_id_;
  std::string enrollment_token_;
  DMToken dm_token_;
  bool should_display_error_message_on_failure_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BrowserDMTokenStorage> weak_factory_{this};
};

}  // namespace policy
#endif  // COMPONENTS_ENTERPRISE_BROWSER_CONTROLLER_BROWSER_DM_TOKEN_STORAGE_H_
