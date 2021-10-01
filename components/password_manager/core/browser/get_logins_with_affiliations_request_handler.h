// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GET_LOGINS_WITH_AFFILIATIONS_REQUEST_HANDLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GET_LOGINS_WITH_AFFILIATIONS_REQUEST_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

#include "components/password_manager/core/browser/password_form_digest.h"

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
      const PasswordFormDigest& form,
      base::WeakPtr<PasswordStoreConsumer> consumer,
      PasswordStoreInterface* store);

  // Returns a OnceCallback that calls 'HandleLoginsForFormReceived()'.
  base::OnceCallback<void(LoginsResult)> LoginsForFormClosure();

  // Returns a OnceCallback that calls 'HandleAffiliationsReceived()'. The
  // callback returns the forms to be additionally requested from the password
  // store.
  base::OnceCallback<
      std::vector<PasswordFormDigest>(const std::vector<std::string>&)>
  AffiliationsClosure();

  // Returns a OnceCallback that calls 'HandleAffiliatedLoginsReceived()'.
  base::OnceCallback<void(LoginsResult)> AffiliatedLoginsClosure();

 private:
  friend class base::RefCounted<GetLoginsWithAffiliationsRequestHandler>;
  ~GetLoginsWithAffiliationsRequestHandler();

  // Appends logins to the 'results_' and calls 'forms_received_'.
  void HandleLoginsForFormReceived(LoginsResult logins);

  // From the affiliated realms returns all the forms to be additionally queried
  // in the password store. The list excludes the PSL matches because those will
  // be already returned by the main request.
  std::vector<PasswordFormDigest> HandleAffiliationsReceived(
      const std::vector<std::string>& realms);

  // Receives all the affiliated logins from the password store.
  void HandleAffiliatedLoginsReceived(
      std::vector<std::unique_ptr<PasswordForm>> logins);

  void NotifyConsumer();

  const PasswordFormDigest requested_digest_;

  // All the affiliations for 'requested_digest_'.
  base::flat_set<std::string> affiliations_;

  base::WeakPtr<PasswordStoreConsumer> consumer_;

  raw_ptr<PasswordStoreInterface> store_;

  // Closure which is released after being called 2 times.
  base::RepeatingClosure forms_received_;

  // PasswordForms to be sent to consumer.
  std::vector<std::unique_ptr<PasswordForm>> results_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GET_LOGINS_WITH_AFFILIATIONS_REQUEST_HANDLER_H_
