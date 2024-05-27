// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_UTILS_H_

#include "base/functional/callback.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/task_runner.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"

namespace password_manager {

struct SingleLookupResponse;

enum class LeakDetectionInitiator {
  kSignInCheck = 0,
  kBulkSyncedPasswordsCheck = 1,
  kEditCheck = 2,
  kIGABulkSyncedPasswordsCheck = 3,
  kClientUseCaseUnspecified = 4,
  kDesktopProactivePasswordCheckup = 5,
  kIosProactivePasswordCheckup = 6,
  kMaxValue = kIosProactivePasswordCheckup,
};

// Contains the payload for analysing one credential against the leaks.
struct LookupSingleLeakPayload {
  LeakDetectionInitiator initiator;
  std::string username_hash_prefix;
  std::string encrypted_payload;
};

// Stores all the data needed for one credential lookup.
struct LookupSingleLeakData {
  LookupSingleLeakData() = default;
  LookupSingleLeakData(LookupSingleLeakData&& other) = default;
  LookupSingleLeakData& operator=(LookupSingleLeakData&& other) = default;
  ~LookupSingleLeakData() = default;

  LookupSingleLeakData(const LookupSingleLeakData&) = delete;
  LookupSingleLeakData& operator=(const LookupSingleLeakData&) = delete;

  LookupSingleLeakPayload payload;

  std::string encryption_key;
};

using SingleLeakRequestDataCallback =
    base::OnceCallback<void(LookupSingleLeakData)>;

// Describes possible results of analyzing a leak response from the server.
// Needs to stay in sync with the PasswordAnalyzeLeakResponseResult enum in
// enums.xml.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AnalyzeResponseResult {
  kDecryptionError = 0,
  kNotLeaked = 1,
  kLeaked = 2,
  kMaxValue = kLeaked,
};

using SingleLeakResponseAnalysisCallback =
    base::OnceCallback<void(AnalyzeResponseResult)>;

// Asynchronously creates a data payload for single credential check.
// Callback is invoked on the calling thread with the protobuf and the
// encryption key used.
void PrepareSingleLeakRequestData(LeakDetectionInitiator initiator,
                                  const std::string& username,
                                  const std::string& password,
                                  SingleLeakRequestDataCallback callback);

// Asynchronously creates a data payload for a credential check with the given
// encryption key. The task is posted to |task_runner| via |task_tracker|.
// Callback is invoked on the calling thread with the protobuf.
void PrepareSingleLeakRequestData(
    base::CancelableTaskTracker& task_tracker,
    base::TaskRunner& task_runner,
    LeakDetectionInitiator initiator,
    const std::string& encryption_key,
    const std::string& username,
    const std::string& password,
    base::OnceCallback<void(LookupSingleLeakPayload)> callback);

// Analyses the |response| asynchronously and checks if the credential was
// leaked. |callback| is invoked on the calling thread.
void AnalyzeResponse(std::unique_ptr<SingleLookupResponse> response,
                     const std::string& encryption_key,
                     SingleLeakResponseAnalysisCallback callback);

// Requests an access token for the API. |callback| is to be called with the
// result. The caller should keep the returned fetcher alive.
[[nodiscard]] std::unique_ptr<signin::AccessTokenFetcher> RequestAccessToken(
    signin::IdentityManager* identity_manager,
    signin::AccessTokenFetcher::TokenCallback callback);

// Checks if for given initiator a backend notification should be triggered for
// newly detected leaked credentials.
TriggerBackendNotification ShouldTriggerBackendNotificationForInitiator(
    LeakDetectionInitiator initiator);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_UTILS_H_
