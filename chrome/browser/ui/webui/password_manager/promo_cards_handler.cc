// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_cards_handler.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/password_manager/promo_card.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/webui/password_manager/promo_cards/access_on_any_device_promo.h"
#include "chrome/browser/ui/webui/password_manager/promo_cards/move_passwords_promo.h"
#include "chrome/browser/ui/webui/password_manager/promo_cards/password_checkup_promo.h"
#include "chrome/browser/ui/webui/password_manager/promo_cards/password_manager_shortcut_promo.h"
#include "chrome/browser/ui/webui/password_manager/promo_cards/web_password_manager_promo.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/webui/password_manager/promo_cards/relaunch_chrome_promo.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/password_manager/promo_cards/screenlock_reauth_promo.h"
#endif

namespace password_manager {

namespace {

// Returns the base::Value associated with the promo card.
base::Value::Dict PromoCardToValueDict(
    const PasswordPromoCardBase* promo_card) {
  base::Value::Dict dict;
  dict.Set("id", promo_card->GetPromoID());
  dict.Set("title", promo_card->GetTitle());
  dict.Set("description", promo_card->GetDescription());
  if (!promo_card->GetActionButtonText().empty()) {
    dict.Set("actionButtonText", promo_card->GetActionButtonText());
  }
  return dict;
}

std::vector<std::unique_ptr<PasswordPromoCardBase>> GetAllPromoCardsForProfile(
    Profile* profile) {
  std::vector<std::unique_ptr<PasswordPromoCardBase>> promo_cards;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  promo_cards.push_back(std::make_unique<PasswordCheckupPromo>(
      profile->GetPrefs(),
      extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(profile,
                                                                        false)
          .get()));
  promo_cards.push_back(std::make_unique<WebPasswordManagerPromo>(
      profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile)));
  promo_cards.push_back(
      std::make_unique<PasswordManagerShortcutPromo>(profile));
  promo_cards.push_back(
      std::make_unique<AccessOnAnyDevicePromo>(profile->GetPrefs()));
  promo_cards.push_back(std::make_unique<MovePasswordsPromo>(
      profile,
      extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(profile,
                                                                        false)
          .get()));
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  promo_cards.push_back(
      std::make_unique<RelaunchChromePromo>(profile->GetPrefs()));
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
  promo_cards.push_back(std::make_unique<ScreenlockReauthPromo>(profile));
#endif
  return promo_cards;
}

}  // namespace

PromoCardsHandler::PromoCardsHandler(Profile* profile) : profile_(profile) {
  promo_cards_ = GetAllPromoCardsForProfile(profile_);
}

PromoCardsHandler::PromoCardsHandler(
    base::PassKey<class PromoCardsHandlerTest>,
    Profile* profile,
    std::vector<std::unique_ptr<PasswordPromoCardBase>> promo_cards)
    : profile_(profile), promo_cards_(std::move(promo_cards)) {}

PromoCardsHandler::~PromoCardsHandler() = default;

void PromoCardsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAvailablePromoCard",
      base::BindRepeating(&PromoCardsHandler::HandleGetAvailablePromoCard,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "recordPromoDismissed",
      base::BindRepeating(&PromoCardsHandler::HandleRecordPromoDismissed,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "restartBrowser", base::BindRepeating(&PromoCardsHandler::RestartChrome,
                                            base::Unretained(this)));
}

void PromoCardsHandler::RestartChrome(const base::Value::List& args) {
  chrome::AttemptRestart();
}

void PromoCardsHandler::HandleGetAvailablePromoCard(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  PasswordPromoCardBase* promo_card_to_show = GetPromoToShowAndUpdatePref();
  if (promo_card_to_show) {
    ResolveJavascriptCallback(callback_id,
                              PromoCardToValueDict(promo_card_to_show));
  } else {
    ResolveJavascriptCallback(callback_id, base::Value());
  }
}

void PromoCardsHandler::HandleRecordPromoDismissed(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  std::string promo_id = args[0].GetString();

  for (auto& promo_card : promo_cards_) {
    if (promo_card->GetPromoID() == promo_id) {
      promo_card->OnPromoCardDismissed();
      return;
    }
  }
}

PasswordPromoCardBase* PromoCardsHandler::GetPromoToShowAndUpdatePref() {
  std::vector<PasswordPromoCardBase*> promo_card_to_show_candidates;
  for (const auto& promo_card : promo_cards_) {
    if (promo_card->ShouldShowPromo()) {
      // If there's a reason to show relaunch Chrome bubble, it should take the
      // highest priority.
      if (promo_card->GetPromoCardType() == PromoCardType::kRelauchChrome) {
        promo_card->OnPromoCardShown();
        return promo_card.get();
      }
      promo_card_to_show_candidates.push_back(promo_card.get());
    }
  }
  if (promo_card_to_show_candidates.empty()) {
    return nullptr;
  }
  // Sort based on last time shown.
  auto* promo_to_show = *base::ranges::min_element(
      promo_card_to_show_candidates, [](auto* lhs, auto* rhs) {
        return lhs->last_time_shown() < rhs->last_time_shown();
      });

  promo_to_show->OnPromoCardShown();
  return promo_to_show;
}

}  // namespace password_manager
