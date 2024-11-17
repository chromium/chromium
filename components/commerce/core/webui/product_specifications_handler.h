// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_WEBUI_PRODUCT_SPECIFICATIONS_HANDLER_H_
#define COMPONENTS_COMMERCE_CORE_WEBUI_PRODUCT_SPECIFICATIONS_HANDLER_H_

#include "base/scoped_observation.h"
#include "components/commerce/core/mojom/product_specifications.mojom.h"
#include "components/commerce/core/mojom/shopping_service.mojom.h"
#include "components/commerce/core/product_specifications/product_specifications_service.h"
#include "components/commerce/core/product_specifications/product_specifications_set.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace commerce {

class ProductSpecificationsHandler
    : public product_specifications::mojom::ProductSpecificationsHandler,
      public ProductSpecificationsSet::Observer {
 public:
  // Handles platform specific tasks.
  class Delegate {
   public:
    Delegate() = default;
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual void ShowDisclosureDialog(const std::vector<GURL>& urls,
                                      const std::string& name,
                                      const std::string& set_id) = 0;

    virtual void ShowProductSpecificationsSetForUuid(const base::Uuid& uuid,
                                                     bool in_new_tab) = 0;

    virtual void ShowSyncSetupFlow() = 0;
  };

  explicit ProductSpecificationsHandler(
      mojo::PendingRemote<product_specifications::mojom::Page> remote_page,
      mojo::PendingReceiver<
          product_specifications::mojom::ProductSpecificationsHandler> receiver,
      std::unique_ptr<Delegate> delegate,
      history::HistoryService* history_service,
      PrefService* pref_service,
      ProductSpecificationsService* product_specs_service);
  ProductSpecificationsHandler(const ProductSpecificationsHandler&) = delete;
  ProductSpecificationsHandler& operator=(const ProductSpecificationsHandler&) =
      delete;
  ~ProductSpecificationsHandler() override;

  // product_specifications::mojom::ProductSpecificationsHandler
  void MaybeShowDisclosure(const std::vector<GURL>& urls,
                           const std::string& name,
                           const std::string& set_id,
                           MaybeShowDisclosureCallback callback) override;
  void SetAcceptedDisclosureVersion(
      product_specifications::mojom::DisclosureVersion) override;
  void DeclineDisclosure() override;
  void ShowSyncSetupFlow() override;
  void ShowProductSpecificationsSetForUuid(const base::Uuid& uuid,
                                           bool in_new_tab) override;
  void GetPageTitleFromHistory(
      const GURL& url,
      GetPageTitleFromHistoryCallback callback) override;

  // product_specifications::mojom::Page
  void OnProductSpecificationsSetAdded(
      const ProductSpecificationsSet& set) override;
  void OnProductSpecificationsSetRemoved(
      const ProductSpecificationsSet& set) override;
  void OnProductSpecificationsSetUpdate(
      const ProductSpecificationsSet& before,
      const ProductSpecificationsSet& set) override;

 private:
  mojo::Remote<product_specifications::mojom::Page> remote_page_;
  mojo::Receiver<product_specifications::mojom::ProductSpecificationsHandler>
      receiver_;
  std::unique_ptr<Delegate> delegate_;

  const raw_ptr<history::HistoryService> history_service_;

  raw_ptr<PrefService> pref_service_;

  base::ScopedObservation<ProductSpecificationsService,
                          ProductSpecificationsSet::Observer>
      scoped_product_specs_observer_{this};

  // Used for history service queries on URLs.
  base::CancelableTaskTracker cancelable_task_tracker_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_WEBUI_PRODUCT_SPECIFICATIONS_HANDLER_H_
