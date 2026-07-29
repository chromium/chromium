// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_cards_handler.h"

#include <algorithm>
#include <memory>

#include "base/functional/bind.h"
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

#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

namespace {

constexpr char kIdKey[] = "id";
constexpr char kLastTimeShownKey[] = "last_time_shown";
constexpr char kNumberOfTimesShownKey[] = "number_of_times_shown";
constexpr char kWasDismissedKey[] = "was_dismissed";

base::DictValue CreateNotificationCardPrefEntry(const std::string& id) {
  base::DictValue entry;
  entry.Set(kIdKey, id);
  entry.Set(kLastTimeShownKey, base::TimeToValue(base::Time()));
  entry.Set(kNumberOfTimesShownKey, 0);
  entry.Set(kWasDismissedKey, false);
  return entry;
}

NotificationCardPrefState GetCardPrefState(PrefService* prefs,
                                           const std::string& id) {
  NotificationCardPrefState state;
  const base::ListValue& card_prefs =
      prefs->GetList(prefs::kPasswordManagerPromoCardsList);
  for (const auto& card_pref : card_prefs) {
    const std::string* card_id = card_pref.GetDict().FindString(kIdKey);
    if (card_id == nullptr || *card_id != id) {
      continue;
    }
    state.number_of_times_shown =
        card_pref.GetDict().FindInt(kNumberOfTimesShownKey).value_or(0);
    state.last_time_shown =
        base::ValueToTime(card_pref.GetDict().Find(kLastTimeShownKey))
            .value_or(base::Time());
    state.was_dismissed =
        card_pref.GetDict().FindBool(kWasDismissedKey).value_or(false);
    return state;
  }
  return state;
}

void MarkCardShown(PrefService* prefs,
                   const std::string& id,
                   NotificationCardType type) {
  ScopedListPrefUpdate update(prefs, prefs::kPasswordManagerPromoCardsList);
  bool found = false;
  for (auto& notification_card_pref : update.Get()) {
    const std::string* card_id =
        notification_card_pref.GetDict().FindString(kIdKey);
    if (card_id && *card_id == id) {
      int times_shown = notification_card_pref.GetDict()
                            .FindInt(kNumberOfTimesShownKey)
                            .value_or(0);
      notification_card_pref.GetDict().Set(kNumberOfTimesShownKey,
                                           times_shown + 1);
      notification_card_pref.GetDict().Set(
          kLastTimeShownKey, base::TimeToValue(base::Time::Now()));
      found = true;
      break;
    }
  }
  if (!found) {
    base::DictValue entry = CreateNotificationCardPrefEntry(id);
    entry.Set(kNumberOfTimesShownKey, 1);
    entry.Set(kLastTimeShownKey, base::TimeToValue(base::Time::Now()));
    update.Get().Append(std::move(entry));
  }
  base::UmaHistogramEnumeration("PasswordManager.PromoCard.Shown", type);
}

void MarkCardDismissed(PrefService* prefs, const std::string& id) {
  ScopedListPrefUpdate update(prefs, prefs::kPasswordManagerPromoCardsList);
  for (auto& notification_card_pref : update.Get()) {
    const std::string* card_id =
        notification_card_pref.GetDict().FindString(kIdKey);
    if (card_id && *card_id == id) {
      notification_card_pref.GetDict().Set(kWasDismissedKey, true);
      return;
    }
  }
  base::DictValue entry = CreateNotificationCardPrefEntry(id);
  entry.Set(kWasDismissedKey, true);
  update.Get().Append(std::move(entry));
}

using NotificationCardCandidate =
    std::pair<PasswordNotificationCardBase*, NotificationCardPrefState>;

bool CompareNotificationCardCandidates(const NotificationCardCandidate& lhs,
                                       const NotificationCardCandidate& rhs) {
  auto l_type = lhs.first->GetNotificationSeverity();
  auto r_type = rhs.first->GetNotificationSeverity();
  if (l_type != r_type) {
    if (l_type == NotificationSeverity::kCritical) {
      return true;
    }
    return false;
  }

  if (lhs.second.last_time_shown != rhs.second.last_time_shown) {
    return lhs.second.last_time_shown < rhs.second.last_time_shown;
  }

  return lhs.first->GetCardID() < rhs.first->GetCardID();
}

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
  dict.Set("isDismissible", notification_card->IsDismissible());
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
      SyncServiceFactory::GetForProfile(profile)));
  notification_cards_.push_back(
      std::make_unique<PasswordManagerShortcutPromo>(profile));
  notification_cards_.push_back(std::make_unique<AccessOnAnyDevicePromo>());
#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS)
  notification_cards_.push_back(std::make_unique<MovePasswordsPromo>(
      profile,
      extensions::PasswordsPrivateDelegateFactory::GetForBrowserContext(profile,
                                                                        false)
          .get()));
#endif
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  auto relaunch_banner = std::make_unique<RelaunchChromeBanner>();
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
      if (!notification_card->IsDismissible()) {
        return;
      }
      MarkCardDismissed(profile_->GetPrefs(), card_id);
      return;
    }
  }
}

PasswordNotificationCardBase*
NotificationCardsHandler::GetNotificationCardToShowAndUpdatePref() {
  std::vector<NotificationCardCandidate> candidates;
  for (const auto& card : notification_cards_) {
    NotificationCardPrefState state =
        GetCardPrefState(profile_->GetPrefs(), card->GetCardID());
    if (card->ShouldShowCard(state)) {
      candidates.emplace_back(card.get(), state);
    }
  }
  if (candidates.empty()) {
    return nullptr;
  }
  auto it =
      std::ranges::min_element(candidates, CompareNotificationCardCandidates);

  PasswordNotificationCardBase* card_to_show = it->first;
  MarkCardShown(profile_->GetPrefs(), card_to_show->GetCardID(),
                card_to_show->GetNotificationCardType());
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
