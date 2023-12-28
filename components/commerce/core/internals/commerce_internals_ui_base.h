// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_INTERNALS_COMMERCE_INTERNALS_UI_BASE_H_
#define COMPONENTS_COMMERCE_CORE_INTERNALS_COMMERCE_INTERNALS_UI_BASE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/commerce/core/internals/commerce_internals_handler.h"
#include "components/commerce/core/internals/mojom/commerce_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/webui/resource_path.h"

namespace commerce {

class ShoppingService;

// The base implementation for the class that helps build and initialize webui
// on specific platforms (split between "content" and iOS).
class CommerceInternalsUIBase : public mojom::CommerceInternalsHandlerFactory {
 public:
  explicit CommerceInternalsUIBase(ShoppingService* shopping_service);
  CommerceInternalsUIBase(const CommerceInternalsUIBase&) = delete;
  CommerceInternalsUIBase operator&(const CommerceInternalsUIBase&) = delete;
  ~CommerceInternalsUIBase() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<mojom::CommerceInternalsHandlerFactory> receiver);

  // commerce::mojom::CommerceInternalsHandlerFactory
  void CreateCommerceInternalsHandler(
      mojo::PendingRemote<mojom::CommerceInternalsPage> page,
      mojo::PendingReceiver<mojom::CommerceInternalsHandler> receiver) override;

 private:
  std::unique_ptr<CommerceInternalsHandler> page_handler_;

  mojo::Receiver<mojom::CommerceInternalsHandlerFactory> page_factory_receiver_{
      this};

  // The shopping service should always outlive this object since it is tied to
  // the browser's lifecycle and this UI object is tied to the current tab.
  raw_ptr<ShoppingService> shopping_service_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_INTERNALS_COMMERCE_INTERNALS_UI_BASE_H_
