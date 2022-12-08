// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FAMILY_LINK_USER_INTERNALS_FAMILY_LINK_USER_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FAMILY_LINK_USER_INTERNALS_FAMILY_LINK_USER_INTERNALS_MESSAGE_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/supervised_user/supervised_user_error_page/supervised_user_error_page.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "content/public/browser/web_ui_message_handler.h"

// The implementation for the chrome://family-link-user-internals page.
class FamilyLinkUserInternalsMessageHandler
    : public content::WebUIMessageHandler,
      public SupervisedUserServiceObserver,
      public SupervisedUserURLFilter::Observer {
 public:
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

  // SupervisedUserServiceObserver:
  void OnURLFilterChanged() override;

  SupervisedUserService* GetSupervisedUserService();

  void HandleRegisterForEvents(const base::Value::List& args);
  void HandleGetBasicInfo(const base::Value::List& args);
  void HandleTryURL(const base::Value::List& args);

  void SendBasicInfo();
  void SendFamilyLinkUserSettings(const base::Value::Dict& settings);

  void OnTryURLResult(
      const std::string& callback_id,
      SupervisedUserURLFilter::FilteringBehavior behavior,
      supervised_user_error_page::FilteringBehaviorReason reason,
      bool uncertain);

  // SupervisedUserURLFilter::Observer:
  void OnSiteListUpdated() override;
  void OnURLChecked(const GURL& url,
                    SupervisedUserURLFilter::FilteringBehavior behavior,
                    supervised_user_error_page::FilteringBehaviorReason reason,
                    bool uncertain) override;

  base::CallbackListSubscription user_settings_subscription_;

  base::ScopedObservation<SupervisedUserURLFilter,
                          SupervisedUserURLFilter::Observer>
      scoped_observation_{this};

  base::WeakPtrFactory<FamilyLinkUserInternalsMessageHandler> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_FAMILY_LINK_USER_INTERNALS_FAMILY_LINK_USER_INTERNALS_MESSAGE_HANDLER_H_
