// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/internals/commerce_internals_ui_base.h"

#include "components/commerce/core/shopping_service.h"

namespace commerce {

CommerceInternalsUIBase::CommerceInternalsUIBase(
    ShoppingService* shopping_service)
    : shopping_service_(shopping_service) {}

CommerceInternalsUIBase::~CommerceInternalsUIBase() = default;

void CommerceInternalsUIBase::BindInterface(
    mojo::PendingReceiver<mojom::CommerceInternalsHandlerFactory> receiver) {
  if (page_factory_receiver_.is_bound()) {
    page_factory_receiver_.reset();
  }
  if (receiver.is_valid()) {
    page_factory_receiver_.Bind(std::move(receiver));
  }
}

void CommerceInternalsUIBase::CreateCommerceInternalsHandler(
    mojo::PendingRemote<mojom::CommerceInternalsPage> page,
    mojo::PendingReceiver<mojom::CommerceInternalsHandler> receiver) {
  page_handler_ = std::make_unique<CommerceInternalsHandler>(
      std::move(page), std::move(receiver), shopping_service_.get());
}

}  // namespace commerce
