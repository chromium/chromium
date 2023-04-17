// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/internals/commerce_internals_handler.h"

#include "components/commerce/core/shopping_service.h"

namespace commerce {

CommerceInternalsHandler::CommerceInternalsHandler(
    mojo::PendingRemote<mojom::CommerceInternalsPage> page,
    mojo::PendingReceiver<mojom::CommerceInternalsHandler> receiver,
    ShoppingService* shopping_service)
    : page_(std::move(page)),
      receiver_(this, std::move(receiver)),
      shopping_service_(shopping_service) {
  page_->OnShoppingListEligibilityChanged(
      shopping_service_ ? shopping_service_->IsShoppingListEligible() : false);
}

CommerceInternalsHandler::~CommerceInternalsHandler() = default;

void CommerceInternalsHandler::GetIsShoppingListEligible(
    GetIsShoppingListEligibleCallback callback) {
  std::move(callback).Run(
      shopping_service_ ? shopping_service_->IsShoppingListEligible() : false);
}

}  // namespace commerce
