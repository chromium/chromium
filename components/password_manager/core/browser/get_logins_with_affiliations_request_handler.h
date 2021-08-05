// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GET_LOGINS_WITH_AFFILIATIONS_REQUEST_HANDLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GET_LOGINS_WITH_AFFILIATIONS_REQUEST_HANDLER_H_

#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

namespace password_manager {

class PasswordStoreInterface;
class PasswordStoreConsumer;
struct PasswordForm;

// Helper object which is used to obtain PasswordForms for a particular login
// and affiliated logins parallelly. 'consumer' is notified about result
// afterwards.
class GetLoginsWithAffiliationsRequestHandler
    : public base::RefCounted<GetLoginsWithAffiliationsRequestHandler> {
 public:
  using LoginsResult = std::vector<std::unique_ptr<PasswordForm>>;

  GetLoginsWithAffiliationsRequestHandler(
      base::WeakPtr<PasswordStoreConsumer> consumer,
      PasswordStoreInterface* store);

  // Returns a OnceCallback that calls 'HandleLoginsForFormReceived()'.
  base::OnceCallback<void(LoginsResult)> LoginsForFormClosure();

  // Returns a OnceCallback that calls 'HandleAffiliatedLoginsReceived()'.
  base::OnceCallback<void(LoginsResult)> AffiliatedLoginsClosure();

 private:
  friend class base::RefCounted<GetLoginsWithAffiliationsRequestHandler>;
  ~GetLoginsWithAffiliationsRequestHandler();

  // Appends logins to the 'results_' and calls 'forms_received_'.
  void HandleLoginsForFormReceived(LoginsResult logins);

  // Marks logins as affiliated and trims username only. Afterwards appends
  // result to 'results_' and calls 'forms_received_'.
  void HandleAffiliatedLoginsReceived(LoginsResult logins);

  void NotifyConsumer();

  base::WeakPtr<PasswordStoreConsumer> consumer_;

  PasswordStoreInterface* store_;

  // Closure which is released after being called 2 times.
  base::RepeatingClosure forms_received_;

  // PasswordForms to be sent to consumer.
  std::vector<std::unique_ptr<PasswordForm>> results_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GET_LOGINS_WITH_AFFILIATIONS_REQUEST_HANDLER_H_
