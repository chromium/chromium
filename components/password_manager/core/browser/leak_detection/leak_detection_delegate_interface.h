// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_DELEGATE_INTERFACE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_DELEGATE_INTERFACE_H_

#include "base/types/strong_alias.h"
#include "url/gurl.h"

namespace password_manager {

class LeakCheckCredential;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Needs to stay in sync with PasswordLeakDetectionError in enums.xml.
enum class LeakDetectionError {
  // The user isn't signed-in to Chrome.
  kNotSignIn = 0,
  // Error obtaining a token.
  kTokenRequestFailure = 1,
  // Error in hashing/encrypting for the request.
  kHashingFailure = 2,
  // Error obtaining a valid server response.
  kInvalidServerResponse = 3,
  // Error related to network connection.
  kNetworkError = 4,
  // The user ran out of quota.
  kQuotaLimit = 5,
  kMaxValue = kQuotaLimit,
};

using IsLeaked = base::StrongAlias<class IsLeakedTag, bool>;

// Interface with callbacks for LeakDetectionCheck. Used to get the result of
// the check.
class LeakDetectionDelegateInterface {
 public:
  LeakDetectionDelegateInterface() = default;
  virtual ~LeakDetectionDelegateInterface() = default;

  // Not copyable or movable
  LeakDetectionDelegateInterface(const LeakDetectionDelegateInterface&) =
      delete;
  LeakDetectionDelegateInterface& operator=(
      const LeakDetectionDelegateInterface&) = delete;
  LeakDetectionDelegateInterface(LeakDetectionDelegateInterface&&) = delete;
  LeakDetectionDelegateInterface& operator=(LeakDetectionDelegateInterface&&) =
      delete;

  // Called when the request is finished without error.
  // |leak| is true iff the checked credential was leaked.
  // |url| and |username| are taken from Start() for presentation in the UI.
  // Pass parameters by value because the caller can be destroyed here.
  virtual void OnLeakDetectionDone(bool is_leaked,
                                   GURL url,
                                   std::u16string username,
                                   std::u16string password) = 0;

  virtual void OnError(LeakDetectionError error) = 0;
};

// Delegate for BulkLeakCheck. Gets the updates during processing the list.
class BulkLeakCheckDelegateInterface {
 public:
  BulkLeakCheckDelegateInterface() = default;
  virtual ~BulkLeakCheckDelegateInterface() = default;

  // Not copyable or movable
  BulkLeakCheckDelegateInterface(const BulkLeakCheckDelegateInterface&) =
      delete;
  BulkLeakCheckDelegateInterface& operator=(
      const BulkLeakCheckDelegateInterface&) = delete;
  BulkLeakCheckDelegateInterface(BulkLeakCheckDelegateInterface&&) = delete;
  BulkLeakCheckDelegateInterface& operator=(BulkLeakCheckDelegateInterface&&) =
      delete;

  // Called when |credential| was processed. |is_leaked| is true if it's leaked.
  virtual void OnFinishedCredential(LeakCheckCredential credential,
                                    IsLeaked is_leaked) = 0;

  // Called when error occurred on one of the credentials. Other credentials are
  // processed further.
  // BulkLeakCheck can be deleted from this call safely.
  virtual void OnError(LeakDetectionError error) = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_DELEGATE_INTERFACE_H_
