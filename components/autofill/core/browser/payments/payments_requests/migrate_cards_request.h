// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_MIGRATE_CARDS_REQUEST_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_MIGRATE_CARDS_REQUEST_H_

#include <string>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

namespace base {
class Value;
}  // namespace base

namespace autofill::payments {

class MigrateCardsRequest : public PaymentsRequest {
 public:
  MigrateCardsRequest(
      const PaymentsNetworkInterface::MigrationRequestDetails& request_details,
      base::span<const MigratableCreditCard> migratable_credit_cards,
      const bool full_sync_enabled,
      MigrateCardsCallback callback);
  MigrateCardsRequest(const MigrateCardsRequest&) = delete;
  MigrateCardsRequest& operator=(const MigrateCardsRequest&) = delete;
  ~MigrateCardsRequest() override;

  // PaymentsRequest:
  std::string GetRequestUrlPath() override;
  std::string GetRequestContentType() override;
  std::string GetRequestContent() override;
  void ParseResponse(const base::Value::Dict& response) override;
  bool IsResponseComplete() override;
  void RespondToDelegate(
      PaymentsAutofillClient::PaymentsRpcResult result) override;

 private:
  // Return the pan field name for the encrypted pan based on the |index|.
  std::string GetPanFieldName(const size_t& index);

  // Return the formatted pan to append to the end of the request.
  std::string GetAppendPan(const CreditCard& credit_card,
                           const std::string& app_locale,
                           const std::string& pan_field_name);

  const PaymentsNetworkInterface::MigrationRequestDetails request_details_;
  const std::vector<MigratableCreditCard> migratable_credit_cards_;
  const bool full_sync_enabled_;
  MigrateCardsCallback callback_;
  std::unique_ptr<std::unordered_map<std::string, std::string>> save_result_;
  std::string display_text_;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_REQUESTS_MIGRATE_CARDS_REQUEST_H_
