// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_HANDLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;
class RelaunchChromeBanner;

namespace os_crypt_async {
class Encryptor;
}  // namespace os_crypt_async

namespace password_manager {

class PasswordNotificationCardBase;
enum class NotificationCardType;

// A class allowing providing PasswordManager WebUI capability to dynamically
// display actionable notification cards depending on the current account
// preferences and already seen cards.
class NotificationCardsHandler : public content::WebUIMessageHandler {
 public:
  explicit NotificationCardsHandler(Profile* profile);
  NotificationCardsHandler(
      base::PassKey<class NotificationCardsHandlerTest>,
      Profile* profile,
      std::vector<std::unique_ptr<PasswordNotificationCardBase>>
          notification_cards);

  NotificationCardsHandler(const NotificationCardsHandler&) = delete;
  NotificationCardsHandler& operator=(const NotificationCardsHandler&) = delete;

  ~NotificationCardsHandler() override;

 private:
  // WebUIMessageHandler:
  void RegisterMessages() override;

  void RestartChrome(const base::ListValue& args);
  void HandleGetAvailableNotificationCard(const base::ListValue& args);
  void FinishGetAvailableNotificationCard(const base::Value& callback_id);
  void HandleRecordNotificationDismissed(const base::ListValue& args);

  PasswordNotificationCardBase* GetNotificationCardToShowAndUpdatePref();

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void OnEncryptorReceived(base::Value callback_id,
                           scoped_refptr<os_crypt_async::Encryptor> encryptor);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  raw_ptr<Profile> profile_;

  std::vector<std::unique_ptr<PasswordNotificationCardBase>>
      notification_cards_;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // This points into `notification_cards_`, so should be ordered after it.
  raw_ptr<RelaunchChromeBanner> relaunch_chrome_banner_ = nullptr;

  base::WeakPtrFactory<NotificationCardsHandler> weak_ptr_factory_{this};
#endif
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_NOTIFICATION_CARDS_HANDLER_H_
