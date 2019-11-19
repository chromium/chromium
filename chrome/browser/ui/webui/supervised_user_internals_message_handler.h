// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUPERVISED_USER_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SUPERVISED_USER_INTERNALS_MESSAGE_HANDLER_H_

#include "base/callback_list.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/supervised_user/supervised_user_error_page/supervised_user_error_page.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_observer.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}  // namespace base

// The implementation for the chrome://supervised-user-internals page.
class SupervisedUserInternalsMessageHandler
    : public content::WebUIMessageHandler,
      public SupervisedUserServiceObserver,
      public SupervisedUserURLFilter::Observer {
 public:
  SupervisedUserInternalsMessageHandler();
  ~SupervisedUserInternalsMessageHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // SupervisedUserServiceObserver:
  void OnURLFilterChanged() override;

  SupervisedUserService* GetSupervisedUserService();

  void HandleRegisterForEvents(const base::ListValue* args);
  void HandleGetBasicInfo(const base::ListValue* args);
  void HandleTryURL(const base::ListValue* args);

  void SendBasicInfo();
  void SendSupervisedUserSettings(const base::DictionaryValue* settings);

  void OnTryURLResult(
      const std::map<std::string, base::string16>& whitelists,
      SupervisedUserURLFilter::FilteringBehavior behavior,
      supervised_user_error_page::FilteringBehaviorReason reason,
      bool uncertain);

  // SupervisedUserURLFilter::Observer:
  void OnSiteListUpdated() override;
  void OnURLChecked(const GURL& url,
                    SupervisedUserURLFilter::FilteringBehavior behavior,
                    supervised_user_error_page::FilteringBehaviorReason reason,
                    bool uncertain) override;

  std::unique_ptr<
      base::CallbackList<void(const base::DictionaryValue*)>::Subscription>
      user_settings_subscription_;

  ScopedObserver<SupervisedUserURLFilter, SupervisedUserURLFilter::Observer>
      scoped_observer_{this};

  base::WeakPtrFactory<SupervisedUserInternalsMessageHandler> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserInternalsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SUPERVISED_USER_INTERNALS_MESSAGE_HANDLER_H_
