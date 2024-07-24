// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/payments/offer_notification_handler.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace autofill {

class AutofillClient;
class AutofillOfferData;
class OfferNotificationHandler;
class PersonalDataManager;

// Manages all Autofill related offers. One per browser context. Owned and
// created by the AutofillOfferManagerFactory.
class AutofillOfferManager : public KeyedService,
                             public PaymentsDataManager::Observer {
 public:
  // Mapping from credit card guid id to offer data.
  using CardLinkedOffersMap = std::map<std::string, AutofillOfferData*>;

  AutofillOfferManager(PersonalDataManager* personal_data);
  ~AutofillOfferManager() override;
  AutofillOfferManager(const AutofillOfferManager&) = delete;
  AutofillOfferManager& operator=(const AutofillOfferManager&) = delete;

  // PaymentsDataManager::Observer:
  void OnPaymentsDataChanged() override;

  // Invoked when the navigation happens.
  void OnDidNavigateFrame(AutofillClient& client);

  // Gets a mapping between credit card's guid id and eligible card-linked
  // offers on the |last_committed_primary_main_frame_url|.
  CardLinkedOffersMap GetCardLinkedOffersMap(
      const GURL& last_committed_primary_main_frame_url) const;

  // Returns true only if the domain of |last_committed_primary_main_frame_url|
  // has an offer.
  bool IsUrlEligible(const GURL& last_committed_primary_main_frame_url);

  // Returns the offer that contains the domain of
  // |last_committed_primary_main_frame_url|.
  AutofillOfferData* GetOfferForUrl(
      const GURL& last_committed_primary_main_frame_url);

 private:
  FRIEND_TEST_ALL_PREFIXES(
      AutofillOfferManagerTest,
      CreateCardLinkedOffersMap_ReturnsOnlyCardLinkedOffers);
  FRIEND_TEST_ALL_PREFIXES(AutofillOfferManagerTest, IsUrlEligible);
  friend class OfferNotificationBubbleViewsInteractiveUiTest;
  friend class OfferNotificationControllerAndroidBrowserTest;

  // Queries |personal_data_| to reset the elements of
  // |eligible_merchant_domains_|
  void UpdateEligibleMerchantDomains();

  raw_ptr<PersonalDataManager> personal_data_;

  // This set includes all the eligible domains where offers are applicable.
  // This is used as a local cache and will be updated whenever the data in the
  // database changes.
  std::set<GURL> eligible_merchant_domains_;

  base::ScopedObservation<PaymentsDataManager, PaymentsDataManager::Observer>
      payments_data_manager_observation{this};

  // The handler for offer notification UI. It is a sub-level component of
  // AutofillOfferManager to decide whether to show the offer notification.
  OfferNotificationHandler notification_handler_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_
