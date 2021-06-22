// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_H_

#include <vector>

#include "base/callback_forward.h"
#include "components/password_manager/core/browser/password_form_digest.h"
#include "components/password_manager/core/browser/password_store_change.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

struct PasswordForm;

// The backend is used by the `PasswordStore` to interact with the storage in a
// platform-dependent way (e.g. on Desktop, it calls a local database while on
// Android, it sends requests to a service).
// All methods are required to do their work asynchronously to prevent expensive
// IO operation from possibly blocking the main thread.
class PasswordStoreBackend {
 public:
  using LoginsResult = std::vector<std::unique_ptr<PasswordForm>>;
  using LoginsReply = base::OnceCallback<void(LoginsResult)>;
  using OptionalLoginsReply =
      base::OnceCallback<void(absl::optional<LoginsResult>)>;
  using OptionalStoreChangeListReply =
      base::OnceCallback<void(absl::optional<PasswordStoreChangeList>)>;

  PasswordStoreBackend() = default;
  PasswordStoreBackend(const PasswordStoreBackend&) = delete;
  PasswordStoreBackend(PasswordStoreBackend&&) = delete;
  PasswordStoreBackend& operator=(const PasswordStoreBackend&) = delete;
  PasswordStoreBackend& operator=(PasswordStoreBackend&&) = delete;
  virtual ~PasswordStoreBackend() = default;

  // Returns all PasswordForms with the same or PSL-matched signon_realm as
  // a form in |forms|. If multiple forms are given, those will be concatenated.
  // Callback is called on the main sequence.
  // TODO(crbug.com/1217071): Check whether this needs OptionalLoginsReply, too.
  virtual void FillMatchingLoginsAsync(
      LoginsReply callback,
      const std::vector<PasswordFormDigest>& forms) = 0;

  // For all methods below:
  // TODO(crbug.com/1217071): Make pure virtual.
  // TODO(crbug.com/1217071): Make PasswordStoreImpl implement it like above.
  // TODO(crbug.com/1217071): Move and Update doc from PasswordStore here.
  // TODO(crbug.com/1217071): Delete corresponding Impl method from
  //  PasswordStore and the async method on backend_ instead.

  virtual void AddLoginAsync(OptionalStoreChangeListReply callback,
                             const PasswordForm& form) {}
  virtual void UpdateLoginAsync(OptionalStoreChangeListReply callback,
                                const PasswordForm& form) {}
  virtual void RemoveLoginAsync(OptionalStoreChangeListReply callback,
                                const PasswordForm& form) {}
  virtual void RemoveLoginsByURLAndTimeAsync(
      OptionalStoreChangeListReply callback,
      const base::RepeatingCallback<bool(const GURL&)>& url_filter,
      base::Time delete_begin,
      base::Time delete_end) {}
  virtual void RemoveLoginsCreatedBetweenAsync(
      OptionalStoreChangeListReply callback,
      base::Time delete_begin,
      base::Time delete_end) {}
  virtual void DisableAutoSignInForOriginsAsync(
      OptionalStoreChangeListReply callback,
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {}
  virtual void FillMatchingLoginsByPasswordAsync(
      LoginsReply callback,
      const std::u16string& plain_text_password) {}
  virtual void FillAutofillableLoginsAsync(
      OptionalStoreChangeListReply callback) {}
  virtual void FillBlocklistLoginsAsync(OptionalStoreChangeListReply callback) {
  }
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_BACKEND_H_
