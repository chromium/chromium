// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_cards_handler.h"

#include <algorithm>
#include <memory>

#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_delegate_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/password_manager/notification_card.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/ui/webui/password_manager/notification_cards/access_on_any_device_promo.h"
#include "chrome/browser/ui/webui/password_manager/notification_cards/password_checkup_promo.h"
#include "chrome/browser/ui/webui/password_manager/notification_cards/password_manager_shortcut_promo.h"
#include "chrome/browser/ui/webui/password_manager/notification_cards/web_password_manager_promo.h"
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/password_manager/notification_cards/move_passwords_promo.h"
#endif
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/password_manager/notification_cards/relaunch_chrome_banner.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#endif

namespace password_manager {

namespace {

// Returns the base::Value associated with the notification card.
base::DictValue NotificationCardToValueDict(
    const PasswordNotificationCardBase* notification_card) {
  base::DictValue dict;
  dict.Set("id", notification_card->GetCardID());
  dict.Set("title", notification_card->GetTitle());
  dict.Set("description", notification_card->GetDescription());
  if (!notification_card->GetActionButtonText().empty()) {
    dict.Set("actionButtonText", notification_card->GetActionButtonText());
  }
  return dict;
}

}  // namespace

NotificationCardsHandler::NotificationCardsHandler(Profile* profile)
    : profile_(profile) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  notification_cards_.push_back(std::make_unique<PasswordCheckupPromo>(
      profile->GetPrefs(),
      extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(profile,
                                                                        false)
          .get()));
  notification_cards_.push_back(std::make_unique<WebPasswordManagerPromo>(
      profile->GetPrefs(), SyncServiceFactory::GetForProfile(profile)));
  notification_cards_.push_back(
      std::make_unique<PasswordManagerShortcutPromo>(profile));
  notification_cards_.push_back(
      std::make_unique<AccessOnAnyDevicePromo>(profile->GetPrefs()));
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS)
  notification_cards_.push_back(std::make_unique<MovePasswordsPromo>(
      profile,
      extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(profile,
                                                                        false)
          .get()));
#endif
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  auto relaunch_banner =
      std::make_unique<RelaunchChromeBanner>(profile->GetPrefs());
  relaunch_chrome_banner_ = relaunch_banner.get();
  notification_cards_.push_back(std::move(relaunch_banner));
#endif
}

NotificationCardsHandler::NotificationCardsHandler(
    base::PassKey<class NotificationCardsHandlerTest>,
    Profile* profile,
    std::vector<std::unique_ptr<PasswordNotificationCardBase>>
        notification_cards)
    : profile_(profile), notification_cards_(std::move(notification_cards)) {}

NotificationCardsHandler::~NotificationCardsHandler() = default;

void NotificationCardsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getAvailableNotificationCard",
      base::BindRepeating(
          &NotificationCardsHandler::HandleGetAvailableNotificationCard,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "recordNotificationDismissed",
      base::BindRepeating(
          &NotificationCardsHandler::HandleRecordNotificationDismissed,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "restartBrowser",
      base::BindRepeating(&NotificationCardsHandler::RestartChrome,
                          base::Unretained(this)));
}

void NotificationCardsHandler::RestartChrome(const base::ListValue& args) {
  chrome::AttemptRestart();
}

void NotificationCardsHandler::HandleGetAvailableNotificationCard(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  if (relaunch_chrome_banner_ &&
      !relaunch_chrome_banner_->is_encryption_available().has_value()) {
    g_browser_process->os_crypt_async()->GetInstance(
        base::BindOnce(&NotificationCardsHandler::OnEncryptorReceived,
                       weak_ptr_factory_.GetWeakPtr(), callback_id.Clone()));
    return;
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  FinishGetAvailableNotificationCard(callback_id);
}

void NotificationCardsHandler::FinishGetAvailableNotificationCard(
    const base::Value& callback_id) {
  PasswordNotificationCardBase* notification_card_to_show =
      GetNotificationCardToShowAndUpdatePref();
  if (notification_card_to_show) {
    ResolveJavascriptCallback(
        callback_id, NotificationCardToValueDict(notification_card_to_show));
  } else {
    ResolveJavascriptCallback(callback_id, base::Value());
  }
}

void NotificationCardsHandler::HandleRecordNotificationDismissed(
    const base::ListValue& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const std::string& card_id = args[0].GetString();

  for (auto& notification_card : notification_cards_) {
    if (notification_card->GetCardID() == card_id) {
      notification_card->OnNotificationCardDismissed();
      return;
    }
  }
}

PasswordNotificationCardBase*
NotificationCardsHandler::GetNotificationCardToShowAndUpdatePref() {
  std::vector<PasswordNotificationCardBase*>
      notification_card_to_show_candidates;
  for (const auto& notification_card : notification_cards_) {
    if (notification_card->ShouldShowCard()) {
      // If there's a reason to show relaunch Chrome bubble, it should take the
      // highest priority.
      if (notification_card->GetNotificationCardType() ==
          NotificationCardType::kRelauchChrome) {
        notification_card->OnNotificationCardShown();
        return notification_card.get();
      }
      notification_card_to_show_candidates.push_back(notification_card.get());
    }
  }
  if (notification_card_to_show_candidates.empty()) {
    return nullptr;
  }
  // Sort based on last time shown.
  auto* card_to_show = *std::ranges::min_element(
      notification_card_to_show_candidates, [](auto* lhs, auto* rhs) {
        return lhs->last_time_shown() < rhs->last_time_shown();
      });

  card_to_show->OnNotificationCardShown();
  return card_to_show;
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void NotificationCardsHandler::OnEncryptorReceived(
    base::Value callback_id,
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  relaunch_chrome_banner_->set_is_encryption_available(
      encryptor->IsEncryptionAvailable());
  FinishGetAvailableNotificationCard(callback_id);
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace password_manager
