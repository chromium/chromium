// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_POST_SAVE_COMPROMISED_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_POST_SAVE_COMPROMISED_HELPER_H_

#include <optional>
#include <string>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

class PrefService;

namespace password_manager {

class PasswordStoreInterface;

// Helps to choose a compromised credential bubble after a password was saved.
class PostSaveCompromisedHelper
    : public password_manager::PasswordStoreConsumer {
 public:
  enum class BubbleType {
    // No follow-up bubble should be shown.
    kNoBubble,
    // Last compromised password was updated and the password check completed
    // recently. The user is presumed safe.
    kPasswordUpdatedSafeState,
    // A compromised password was updated and there are more issues to fix.
    kPasswordUpdatedWithMoreToFix,
  };

  // The callback is told which bubble to bring up and how many compromised
  // credentials in total should be still fixed.
  using BubbleCallback = base::OnceCallback<void(BubbleType, size_t)>;

  // |compromised| contains all insecure credentials for the current site.
  // |current_username| is the username that was just saved or updated.
  PostSaveCompromisedHelper(base::span<const PasswordForm> compromised,
                            const std::u16string& current_username);
  ~PostSaveCompromisedHelper() override;

  PostSaveCompromisedHelper(const PostSaveCompromisedHelper&) = delete;
  PostSaveCompromisedHelper& operator=(const PostSaveCompromisedHelper&) =
      delete;

  // Asynchronously queries the password stores for the compromised credentials
  // and notifies |callback| with the result of analysis.
  void AnalyzeLeakedCredentials(PasswordStoreInterface* profile_store,
                                PasswordStoreInterface* account_store,
                                PrefService* prefs,
                                BubbleCallback callback);

  BubbleType bubble_type() const { return bubble_type_; }
  size_t compromised_count() const { return compromised_count_; }

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override;

  void AnalyzeLeakedCredentialsInternal();

  // Contains the entry for the currently leaked credentials if it was leaked.
  std::optional<PasswordForm> current_leak_;
  // Callback to notify the caller about the bubble type.
  BubbleCallback callback_;
  // BubbleType after the callback was executed.
  BubbleType bubble_type_ = BubbleType::kNoBubble;
  // Count of compromised credentials after the callback was executed.
  size_t compromised_count_ = 0;

  // Closure which is released after both PasswordStores reply with results.
  base::RepeatingClosure forms_received_;

  std::vector<std::unique_ptr<PasswordForm>> passwords_;

  base::WeakPtrFactory<PostSaveCompromisedHelper> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_POST_SAVE_COMPROMISED_HELPER_H_
