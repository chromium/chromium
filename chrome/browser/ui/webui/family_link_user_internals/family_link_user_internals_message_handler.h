// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FAMILY_LINK_USER_INTERNALS_FAMILY_LINK_USER_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FAMILY_LINK_USER_INTERNALS_FAMILY_LINK_USER_INTERNALS_MESSAGE_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/supervised_user_url_filtering_service.h"
#include "content/public/browser/web_ui_message_handler.h"

// The implementation for the chrome://family-link-user-internals page.
class FamilyLinkUserInternalsMessageHandler
    : public content::WebUIMessageHandler,
      public supervised_user::SupervisedUserUrlFilteringService::Observer,
      public signin::IdentityManager::Observer {
 public:
  enum class WebContentFilters : bool {
    kDisabled = false,
    kEnabled = true,
  };

  FamilyLinkUserInternalsMessageHandler();

  FamilyLinkUserInternalsMessageHandler(
      const FamilyLinkUserInternalsMessageHandler&) = delete;
  FamilyLinkUserInternalsMessageHandler& operator=(
      const FamilyLinkUserInternalsMessageHandler&) = delete;

  ~FamilyLinkUserInternalsMessageHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

  // signin::IdentityManager::Observer
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info) override;
  void OnErrorStateOfRefreshTokenUpdatedForAccount(
      const CoreAccountInfo& account_info,
      const GoogleServiceAuthError& error,
      signin_metrics::SourceForRefreshTokenOperation token_operation_source)
      override;
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;

  // Uniform handler for all signin::IdentityManager::Observer changes.
  void OnAccountChanged();

  supervised_user::SupervisedUserUrlFilteringService*
  GetSupervisedUserUrlFilteringService();

  void HandleRegisterForEvents(const base::ListValue& args);
  void HandleGetBasicInfo(const base::ListValue& args);
  void HandleTryURL(const base::ListValue& args);

  void SendBasicInfo();
  void SendFamilyLinkUserSettings(const base::DictValue& settings);
  void SendWebContentFiltersInfo();

  void OnTryURLResult(const std::string& callback_id,
                      supervised_user::WebFilteringResult filtering_result);

  // supervised_user::SupervisedUserUrlFilteringService::Observer:
  void OnUrlFilteringServiceChanged() override;
  void OnUrlChecked(
      supervised_user::WebFilteringResult filtering_result) override;

  // Emulates device-level setting that manipulates search or browser content
  // filtering. Available only to non-supervised profiles. Note: if multiple
  // chrome:// pages are open simultaneously, they might override each other.
  // This is safe, but will render web-ui off-sync.
  WebContentFilters search_content_filtering_status_{
      WebContentFilters::kDisabled};
  WebContentFilters browser_content_filtering_status_{
      WebContentFilters::kDisabled};

  base::CallbackListSubscription user_settings_subscription_;
  base::ScopedObservation<
      supervised_user::SupervisedUserUrlFilteringService,
      supervised_user::SupervisedUserUrlFilteringService::Observer>
      url_filtering_service_observation_{this};

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::WeakPtrFactory<FamilyLinkUserInternalsMessageHandler> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_FAMILY_LINK_USER_INTERNALS_FAMILY_LINK_USER_INTERNALS_MESSAGE_HANDLER_H_
