// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_HELPER_H_

#include <memory>
#include <utility>
#include <vector>

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_store_consumer.h"
#include "url/gurl.h"

namespace password_manager {

class LeakDetectionCheck;
class PasswordStore;

// Helper class to asynchronously requests all credentials with
// a specific password from the |PasswordStore|.
class LeakDetectionDelegateHelper : public PasswordStoreConsumer {
 public:
  // Type alias for |callback_|.
  using LeakTypeReply =
      base::OnceCallback<void(CredentialLeakType, GURL, base::string16)>;

  explicit LeakDetectionDelegateHelper(LeakTypeReply callback);
  ~LeakDetectionDelegateHelper() override;

  // Request all credentials with |password| from |store|.
  // Results are password to |OnGetPasswordStoreResults|.
  void GetCredentialLeakType(PasswordStore* store,
                             GURL url,
                             base::string16 username,
                             base::string16 password);

 private:
  // PasswordStoreConsumer:
  // Is called by the |PasswordStore| once all credentials with the specific
  // password are retrieved. Determine the |CredentialLeakType and invokes
  // |callback_| when done.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  LeakTypeReply callback_;
  GURL url_;
  base::string16 username_;
  base::string16 password_;

  // Instances should be neither copyable nor assignable.
  DISALLOW_COPY_AND_ASSIGN(LeakDetectionDelegateHelper);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_HELPER_H_
