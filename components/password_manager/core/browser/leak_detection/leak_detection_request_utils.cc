// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"

#include <string>

#include "base/containers/span.h"
#include "base/debug/dump_without_crashing.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/leak_detection/single_lookup_response.h"
#include "crypto/sha2.h"

namespace password_manager {
namespace {

// Despite the function is short, it executes long. That's why it should be done
// asynchronously.
LookupSingleLeakData PrepareLookupSingleLeakData(const std::string& username,
                                                 const std::string& password) {
  std::string canonicalized_username = CanonicalizeUsername(username);
  LookupSingleLeakData data;
  data.username_hash_prefix = BucketizeUsername(canonicalized_username);
  data.encrypted_payload = CipherEncrypt(
      ScryptHashUsernameAndPassword(canonicalized_username, password),
      &data.encryption_key);
  return data;
}

// Searches |reencrypted_lookup_hash| in the |encrypted_leak_match_prefixes|
// array. |encryption_key| is the original client key used to encrypt the
// payload.
AnalyzeResponseResult CheckIfCredentialWasLeaked(
    std::unique_ptr<SingleLookupResponse> response,
    const std::string& encryption_key) {
  std::string decrypted_username_password =
      CipherDecrypt(response->reencrypted_lookup_hash, encryption_key);
  if (decrypted_username_password.empty()) {
    DLOG(ERROR) << "Can't decrypt data="
                << base::HexEncode(base::as_bytes(
                       base::make_span(response->reencrypted_lookup_hash)));
    return AnalyzeResponseResult::kDecryptionError;
  }

  std::string hash_username_password =
      crypto::SHA256HashString(decrypted_username_password);

  const ptrdiff_t matched_prefixes =
      std::count_if(response->encrypted_leak_match_prefixes.begin(),
                    response->encrypted_leak_match_prefixes.end(),
                    [&hash_username_password](const std::string& prefix) {
                      return base::StartsWith(hash_username_password, prefix,
                                              base::CompareCase::SENSITIVE);
                    });
  switch (matched_prefixes) {
    case 0:
      return AnalyzeResponseResult::kNotLeaked;
    case 1:
      return AnalyzeResponseResult::kLeaked;
    default:
      // In theory this should never happen, due to the API contract the server
      // provides. Generate a crash dump in case it does, so that we get
      // notified.
      base::debug::DumpWithoutCrashing();
      return AnalyzeResponseResult::kLeaked;
  };
}

}  // namespace

void PrepareSingleLeakRequestData(const std::string& username,
                                  const std::string& password,
                                  SingleLeakRequestDataCallback callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&PrepareLookupSingleLeakData, username, password),
      std::move(callback));
}

void AnalyzeResponse(std::unique_ptr<SingleLookupResponse> response,
                     const std::string& encryption_key,
                     SingleLeakResponseAnalysisCallback callback) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CheckIfCredentialWasLeaked, std::move(response),
                     encryption_key),
      std::move(callback));
}

}  // namespace password_manager
