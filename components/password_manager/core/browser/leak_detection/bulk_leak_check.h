// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_H_

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/supports_user_data.h"

namespace password_manager {

// A credential to be checked against the service. Caller can attach any data to
// it.
class LeakCheckCredential : public base::SupportsUserData {
 public:
  LeakCheckCredential(base::string16 username, base::string16 password);
  // Movable.
  LeakCheckCredential(LeakCheckCredential&&);
  LeakCheckCredential& operator=(LeakCheckCredential&&);
  ~LeakCheckCredential() override;

  // Not copyable.
  LeakCheckCredential(const LeakCheckCredential&) = delete;
  LeakCheckCredential& operator=(const LeakCheckCredential&) = delete;

  const base::string16& username() const { return username_; }
  const base::string16& password() const { return password_; }

 private:
  base::string16 username_;
  base::string16 password_;
};

// The class checks a list of credentials against Google service of leaked
// passwords.
// The feature is available to sign-in users only.
class BulkLeakCheck {
 public:
  BulkLeakCheck() = default;
  virtual ~BulkLeakCheck() = default;

  // Not copyable or movable
  BulkLeakCheck(const BulkLeakCheck&) = delete;
  BulkLeakCheck& operator=(const BulkLeakCheck&) = delete;
  BulkLeakCheck(BulkLeakCheck&&) = delete;
  BulkLeakCheck& operator=(BulkLeakCheck&&) = delete;

  // Appends |credentials| to the list of currently checked credentials. If
  // necessary, starts the pipeline.
  // The caller is responsible for deduplication of credentials if it wants to
  // make it efficient.
  virtual void CheckCredentials(
      std::vector<LeakCheckCredential> credentials) = 0;

  // Returns # of pending credentials to check.
  virtual size_t GetPendingChecksCount() const = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_BULK_LEAK_CHECK_H_
