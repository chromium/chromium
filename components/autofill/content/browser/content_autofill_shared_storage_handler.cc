// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_shared_storage_handler.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/browser/autofill_shared_storage.pb.h"
#include "components/autofill/content/common/content_autofill_features.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"

namespace autofill {
namespace {

constexpr char16_t kAutofillServerCardDataSharedStorageKey[] =
    u"browser_autofill_card_data";

std::u16string EncodeServerCardDataForSharedStorage(
    const std::vector<std::unique_ptr<CreditCard>>& server_card_data) {
  AutofillCreditCardList card_list_proto;
  for (auto& card : server_card_data) {
    AutofillCreditCardData* card_data_proto =
        card_list_proto.add_server_cards();
    card_data_proto->set_last_four(base::UTF16ToUTF8(card->LastFourDigits()));
    card_data_proto->set_network(card->network());
    card_data_proto->set_expiration_month(card->expiration_month());
    card_data_proto->set_expiration_year(card->expiration_year());
  }

  return base::UTF8ToUTF16(card_list_proto.SerializeAsString());
}

}  // namespace

ContentAutofillSharedStorageHandler::ContentAutofillSharedStorageHandler(
    storage::SharedStorageManager& shared_storage_manager)
    : shared_storage_manager_(shared_storage_manager) {}

ContentAutofillSharedStorageHandler::~ContentAutofillSharedStorageHandler() =
    default;

void ContentAutofillSharedStorageHandler::OnServerCardDataRefreshed(
    const std::vector<std::unique_ptr<CreditCard>>& server_card_data) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillSharedStorageServerCardData)) {
    return;
  }

  shared_storage_manager_->Set(
      url::Origin::Create(payments::GetBaseSecureUrl()),
      kAutofillServerCardDataSharedStorageKey,
      EncodeServerCardDataForSharedStorage(server_card_data),
      base::BindOnce(&ContentAutofillSharedStorageHandler::
                         OnSharedStorageSetAutofillDataComplete,
                     weak_factory_.GetWeakPtr()),
      storage::SharedStorageDatabase::SetBehavior::kDefault);
}

void ContentAutofillSharedStorageHandler::
    OnSharedStorageSetAutofillDataComplete(
        storage::SharedStorageManager::OperationResult result) {
  // TODO(crbug/1519929): Record metrics.
}

}  // namespace autofill
