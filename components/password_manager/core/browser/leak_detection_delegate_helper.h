// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_HELPER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "url/gurl.h"

namespace password_manager {

class LeakDetectionCheck;
class PasswordStoreInterface;

// Helper class to asynchronously requests all credentials with
// a specific password from the `PasswordStoreInterface`.
class LeakDetectionDelegateHelper : public PasswordStoreConsumer {
 public:
  // Type alias for `callback_`.
  using LeakTypeReply = base::OnceCallback<void(PasswordForm::Store,
                                                IsReused,
                                                GURL,
                                                std::u16string,
                                                std::vector<GURL>)>;

  LeakDetectionDelegateHelper(
      scoped_refptr<PasswordStoreInterface> profile_store,
      scoped_refptr<PasswordStoreInterface> account_store,
      LeakTypeReply callback);

  LeakDetectionDelegateHelper(const LeakDetectionDelegateHelper&) = delete;
  LeakDetectionDelegateHelper& operator=(const LeakDetectionDelegateHelper&) =
      delete;

  ~LeakDetectionDelegateHelper() override;

  // Request all credentials with `password` from the store.
  // Results are passed to `OnGetPasswordStoreResults`.
  void ProcessLeakedPassword(GURL url,
                             std::u16string username,
                             std::u16string password);

 private:
  // PasswordStoreConsumer:
  // Is called by the `PasswordStoreInterface` once all credentials with the
  // specific password are retrieved.
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<PasswordForm>> results) override;

  // Called when all password store results are available. Computes the
  // resulting credential type and invokes `callback_`.
  void ProcessResults();

  scoped_refptr<PasswordStoreInterface> profile_store_;
  scoped_refptr<PasswordStoreInterface> account_store_;
  LeakTypeReply callback_;
  GURL url_;
  std::u16string username_;
  std::u16string password_;

  base::RepeatingClosure barrier_closure_;
  std::vector<std::unique_ptr<PasswordForm>> partial_results_;

  base::WeakPtrFactory<LeakDetectionDelegateHelper> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_HELPER_H_
