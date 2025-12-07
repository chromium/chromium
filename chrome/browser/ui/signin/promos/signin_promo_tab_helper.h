// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIGNIN_PROMOS_SIGNIN_PROMO_TAB_HELPER_H_
#define CHROME_BROWSER_UI_SIGNIN_PROMOS_SIGNIN_PROMO_TAB_HELPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

// Class used to move a piece of autofill data to the account storage
// after a successful sign in.
class SigninPromoTabHelper
    : public signin::IdentityManager::Observer,
      public content::WebContentsUserData<SigninPromoTabHelper> {
 public:
  explicit SigninPromoTabHelper(content::WebContents& web_contents);
  SigninPromoTabHelper(const SigninPromoTabHelper&) = delete;
  SigninPromoTabHelper& operator=(const SigninPromoTabHelper&) = delete;
  ~SigninPromoTabHelper() override;

  // Creates the SigninPromoTabHelper instance if it does not already
  // exist.
  static SigninPromoTabHelper* GetForWebContents(
      content::WebContents& web_contents);

  // Initializes waiting for a sign in event by observing the IdentityManager.
  // If the sign in happens from a tab with the appropriate `access_point`
  // within the `time_limit`, the `completed_callback` will be executed.
  void InitializeCallbackAfterSignIn(
      base::OnceClosure completed_callback,
      signin_metrics::AccessPoint access_point,
      base::TimeDelta time_limit = base::Minutes(50));

  // Returns true if the helper has been initialized for testing.
  bool IsInitializedForTesting() const;

  // Overrides signin::IdentityManager::Observer functions.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  friend class content::WebContentsUserData<SigninPromoTabHelper>;

  // Resets the state of the SigninPromoTabHelper instance.
  void Reset();

  struct ResetableState {
    explicit ResetableState(signin::IdentityManager::Observer* observer);
    ~ResetableState();

    base::ScopedObservation<signin::IdentityManager,
                            signin::IdentityManager::Observer>
        identity_manager_observation_;
    base::OnceClosure completed_callback_;
    signin_metrics::AccessPoint access_point_ =
        signin_metrics::AccessPoint::kUnknown;
    base::Time initialization_time_;
    base::TimeDelta time_limit_;
    bool is_initialized_ = false;
    bool needs_reauth_ = false;
  };

  std::unique_ptr<ResetableState> state_;
  raw_ptr<content::WebContents> web_contents_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_SIGNIN_PROMOS_SIGNIN_PROMO_TAB_HELPER_H_
