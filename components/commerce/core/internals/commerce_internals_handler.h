// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_INTERNALS_COMMERCE_INTERNALS_HANDLER_H_
#define COMPONENTS_COMMERCE_CORE_INTERNALS_COMMERCE_INTERNALS_HANDLER_H_

#include "components/commerce/core/internals/mojom/commerce_internals.mojom.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace commerce {

class ShoppingService;
class ProductSpecificationsSet;

class CommerceInternalsHandler : public mojom::CommerceInternalsHandler {
 public:
  CommerceInternalsHandler(
      mojo::PendingRemote<mojom::CommerceInternalsPage> page,
      mojo::PendingReceiver<mojom::CommerceInternalsHandler> receiver,
      ShoppingService* shopping_service);
  CommerceInternalsHandler(const CommerceInternalsHandler&) = delete;
  CommerceInternalsHandler& operator=(const CommerceInternalsHandler&) = delete;
  ~CommerceInternalsHandler() override;

  // commerce::mojom::CommerceInternalsHandler:
  void GetIsShoppingListEligible(
      GetIsShoppingListEligibleCallback callback) override;
  void GetShoppingListEligibleDetails(
      GetShoppingListEligibleDetailsCallback callback) override;
  void ResetPriceTrackingEmailPref() override;
  void GetProductInfoForUrl(const GURL& url,
                            GetProductInfoForUrlCallback callback) override;
  void GetSubscriptionDetails(GetSubscriptionDetailsCallback callback) override;
  void GetProductSpecificationsDetails(
      GetProductSpecificationsDetailsCallback callback) override;
  void ResetProductSpecifications() override;

 private:
  mojo::Remote<mojom::CommerceInternalsPage> page_;
  mojo::Receiver<mojom::CommerceInternalsHandler> receiver_;

  void DeleteAllProductSpecificationSets(
      const std::vector<ProductSpecificationsSet> sets);
  // The shopping service should always outlive this object since its lifecycle
  // is tied to the browser while this object is tied to a specific tab.
  raw_ptr<ShoppingService> shopping_service_;

  base::WeakPtrFactory<CommerceInternalsHandler> weak_ptr_factory_{this};
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_INTERNALS_COMMERCE_INTERNALS_HANDLER_H_
