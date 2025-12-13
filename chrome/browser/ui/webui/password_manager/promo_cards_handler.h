// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_HANDLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;
class RelaunchChromePromo;

namespace os_crypt_async {
class Encryptor;
}  // namespace os_crypt_async

namespace password_manager {

class PasswordPromoCardBase;

// A class allowing providing PasswordManager WebUI capability to dynamically
// display actionable promo cards depending on the current account preferences
// and already seen promos.
class PromoCardsHandler : public content::WebUIMessageHandler {
 public:
  explicit PromoCardsHandler(Profile* profile);
  PromoCardsHandler(
      base::PassKey<class PromoCardsHandlerTest>,
      Profile* profile,
      std::vector<std::unique_ptr<PasswordPromoCardBase>> promo_cards);

  PromoCardsHandler(const PromoCardsHandler&) = delete;
  PromoCardsHandler& operator=(const PromoCardsHandler&) = delete;

  ~PromoCardsHandler() override;

 private:
  // WebUIMessageHandler:
  void RegisterMessages() override;

  void RestartChrome(const base::Value::List& args);
  void HandleGetAvailablePromoCard(const base::Value::List& args);
  void FinishGetAvailablePromoCard(const base::Value& callback_id);
  void HandleRecordPromoDismissed(const base::Value::List& args);

  PasswordPromoCardBase* GetPromoToShowAndUpdatePref();

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void OnEncryptorReceived(base::Value callback_id,
                           os_crypt_async::Encryptor encryptor);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  raw_ptr<Profile, DanglingUntriaged> profile_;

  std::vector<std::unique_ptr<PasswordPromoCardBase>> promo_cards_;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // This points into `promo_cards_`, so should be ordered after it.
  raw_ptr<RelaunchChromePromo> relaunch_chrome_promo_ = nullptr;

  base::WeakPtrFactory<PromoCardsHandler> weak_ptr_factory_{this};
#endif
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PROMO_CARDS_HANDLER_H_
