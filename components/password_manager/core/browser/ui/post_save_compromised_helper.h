// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_POST_SAVE_COMPROMISED_HELPER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_POST_SAVE_COMPROMISED_HELPER_H_

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/compromised_credentials_table.h"
#include "components/password_manager/core/browser/ui/compromised_credentials_reader.h"

class PrefService;

namespace password_manager {

class PasswordStore;

// Helps to choose a compromised credential bubble after a password was saved.
class PostSaveCompromisedHelper {
 public:
  enum class BubbleType {
    // No follow-up bubble should be shown.
    kNoBubble,
    // Last compromised password was updated and the password check completed
    // recently. The user is presumed safe.
    kPasswordUpdatedSafeState,
    // A compromised password was updated and there are more issues to fix.
    kPasswordUpdatedWithMoreToFix,
    // A random password was saved/updated. There are stored compromised
    // credentials.
    kUnsafeState,
  };

  // The callback is told which bubble to bring up and how many compromised
  // credentials in total should be still fixed.
  using BubbleCallback = base::OnceCallback<void(BubbleType, size_t)>;

  // |compromised| contains all compromised credentials for the current site.
  // |current_username| is the username that was just saved or updated.
  PostSaveCompromisedHelper(
      base::span<const CompromisedCredentials> compromised,
      const base::string16& current_username);
  ~PostSaveCompromisedHelper();

  PostSaveCompromisedHelper(const PostSaveCompromisedHelper&) = delete;
  PostSaveCompromisedHelper& operator=(const PostSaveCompromisedHelper&) =
      delete;

  // Asynchronously queries the password stores for the compromised credentials
  // and notifies |callback| with the result of analysis.
  void AnalyzeLeakedCredentials(PasswordStore* profile_store,
                                PasswordStore* account_store,
                                PrefService* prefs,
                                BubbleCallback callback);

  BubbleType bubble_type() const { return bubble_type_; }
  size_t compromised_count() const { return compromised_count_; }

 private:
  void OnGetAllCompromisedCredentials(
      std::vector<CompromisedCredentials> compromised_credentials);

  // Contains the entry for the currently leaked credentials if it was leaked.
  base::Optional<CompromisedCredentials> current_leak_;
  // Profile prefs.
  PrefService* prefs_ = nullptr;
  // Callback to notify the caller about the bubble type.
  BubbleCallback callback_;
  // BubbleType after the callback was executed.
  BubbleType bubble_type_ = BubbleType::kNoBubble;
  // Count of compromised credentials after the callback was executed.
  size_t compromised_count_ = 0;

  std::unique_ptr<CompromisedCredentialsReader> compromised_credentials_reader_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_POST_SAVE_COMPROMISED_HELPER_H_
