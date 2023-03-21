// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/promo_cards_handler.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/password_manager/promo_card.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/l10n/l10n_util.h"

namespace password_manager {

namespace {

// Returns the base::Value associated with the promo card.
base::Value::Dict PromoCardToValueDict(const PromoCardInterface* promo_card) {
  base::Value::Dict dict;
  dict.Set("id", promo_card->GetPromoID());
  dict.Set("title", promo_card->GetTitle());
  dict.Set("description", promo_card->GetDescription());
  if (!promo_card->GetActionButtonText().empty()) {
    dict.Set("actionButtonText", promo_card->GetActionButtonText());
  }
  return dict;
}

}  // namespace

PromoCardsHandler::PromoCardsHandler(
    Profile* profile,
    std::vector<std::unique_ptr<PromoCardInterface>> promo_cards)
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
}

void PromoCardsHandler::HandleGetAvailablePromoCard(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  PromoCardInterface* promo_card_to_show = GetPromoToShowAndUpdatePref();
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

PromoCardInterface* PromoCardsHandler::GetPromoToShowAndUpdatePref() {
  std::vector<PromoCardInterface*> promo_card_to_show_candidates;
  for (const auto& promo_card : promo_cards_) {
    if (promo_card->ShouldShowPromo()) {
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
