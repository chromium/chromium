// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_shared_storage_handler.h"

#include "base/base64.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "components/autofill/content/browser/autofill_shared_storage.pb.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {
namespace {

constexpr char16_t kAutofillServerCardDataSharedStorageKey[] =
    u"browser_autofill_card_data";

}  // namespace

ContentAutofillSharedStorageHandler::ContentAutofillSharedStorageHandler(
    storage::SharedStorageManager& shared_storage_manager)
    : shared_storage_manager_(shared_storage_manager) {}

ContentAutofillSharedStorageHandler::~ContentAutofillSharedStorageHandler() =
    default;

void ContentAutofillSharedStorageHandler::OnServerCardDataRefreshed(
    const std::vector<std::unique_ptr<CreditCard>>& server_card_data) {
  // Ensure the data is cleared, in case the feature was previously enabled.
  ClearAutofillSharedStorageData();
}

void ContentAutofillSharedStorageHandler::ClearAutofillSharedStorageData() {
  shared_storage_manager_->Delete(payments::GetGooglePayScriptOrigin(),
                                  kAutofillServerCardDataSharedStorageKey,
                                  base::DoNothing());
}

void ContentAutofillSharedStorageHandler::
    OnSharedStorageSetAutofillDataComplete(
        storage::SharedStorageManager::OperationResult result) {
  UMA_HISTOGRAM_ENUMERATION("Autofill.SharedStorageServerCardDataSetResult",
                            result);
}

}  // namespace autofill
