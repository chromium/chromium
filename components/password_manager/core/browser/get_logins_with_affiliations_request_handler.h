// Copyright 2021 The Chromium Authors
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
#include "components/password_manager/core/browser/password_store_backend_error.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace password_manager {

class PasswordStoreInterface;
class PasswordStoreConsumer;
struct PasswordForm;

// Helper object which is used to obtain PasswordForms for a particular login
// and affiliated logins parallelly. 'consumer' is notified about result
// afterwards.
// TODO(crbug.com/1432264): Rewrite the class according to comments in
// crrev.com/c/chromium/src/+/4439696/comment/be22b57e_15a4f8ce/
class GetLoginsWithAffiliationsRequestHandler
    : public base::RefCounted<GetLoginsWithAffiliationsRequestHandler> {
 public:
  using LoginsResult = std::vector<std::unique_ptr<PasswordForm>>;
  using LoginsResultOrError =
      absl::variant<LoginsResult, PasswordStoreBackendError>;

  GetLoginsWithAffiliationsRequestHandler(
      const PasswordFormDigest& form,
      base::WeakPtr<PasswordStoreConsumer> consumer,
      PasswordStoreInterface* store);

  // Returns a OnceCallback that calls 'HandleLoginsForFormReceived()'.
  base::OnceCallback<void(LoginsResultOrError)> LoginsForFormClosure();

  // Returns a OnceCallback that calls 'HandleAffiliationsReceived()'. The
  // callback returns the forms to be additionally requested from the password
  // store.
  base::OnceCallback<
      std::vector<PasswordFormDigest>(const std::vector<std::string>&)>
  AffiliationsClosure();

  // Returns a OnceCallback that calls 'HandleGroupReceived()'. The
  // callback returns the forms to be additionally requested from the password
  // store.
  base::OnceCallback<
      std::vector<PasswordFormDigest>(const std::vector<std::string>&)>
  GroupClosure();

  // Returns a OnceCallback that calls 'HandleNonLoginsReceived()'.
  base::OnceCallback<void(LoginsResultOrError)> NonFormLoginsClosure();

 private:
  friend class base::RefCounted<GetLoginsWithAffiliationsRequestHandler>;
  ~GetLoginsWithAffiliationsRequestHandler();

  // Receives logins or an error if the backend couldn't fetch them.
  // If it received logins, it appends them to the 'results_'. In any case,
  // it calls 'forms_received_' to signal that it received the response from
  // the backend.
  void HandleLoginsForFormReceived(LoginsResultOrError logins_or_error);

  // From the affiliated realms returns all the forms to be additionally queried
  // in the password store. The list excludes the PSL matches because those will
  // be already returned by the main request.
  std::vector<PasswordFormDigest> HandleAffiliationsReceived(
      const std::vector<std::string>& realms);

  // Similar to |HandleAffiliationsReceived|, but only for grouping.
  std::vector<PasswordFormDigest> HandleGroupReceived(
      const std::vector<std::string>& realms);

  // Receives affiliated and group logins from the password store or an error,
  // in case one occurred, processes `logins_or_error` and calls
  // `forms_received_`.
  void HandleNonFormLoginsReceived(LoginsResultOrError logins_or_error);

  void NotifyConsumer();

  const PasswordFormDigest requested_digest_;

  // All the affiliations for 'requested_digest_'.
  base::flat_set<std::string> affiliations_;

  // The group realms for 'requested_digest_'.
  base::flat_set<std::string> group_;

  base::WeakPtr<PasswordStoreConsumer> consumer_;

  raw_ptr<PasswordStoreInterface, FlakyDanglingUntriaged> store_;

  // Closure which is released after being called 2 times (3 in case
  // |kFillingAcrossGroupedSites| is enabled).
  base::RepeatingClosure forms_received_;

  // PasswordForms to be sent to consumer if the backend call made to retrieve
  // them didn't result in an error. If an error was encountered, `results_`
  // will be empty.
  LoginsResult results_;

  // Holds the error encountered by the store backend when fetching saved
  // credentials if such an error has occurred.
  absl::optional<PasswordStoreBackendError> backend_error_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_GET_LOGINS_WITH_AFFILIATIONS_REQUEST_HANDLER_H_
