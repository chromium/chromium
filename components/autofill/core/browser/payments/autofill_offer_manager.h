// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/payments/offer_notification_handler.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace autofill {

class AutofillClient;
class AutofillOfferData;
class OfferNotificationHandler;
class PaymentsDataManager;

// Determines which of the offers stored in the `PaymentsDataManager` apply to
// a page, and drives the offer notification UI for them. One per browser
// context. Owned and created by the AutofillOfferManagerFactory.
class AutofillOfferManager : public KeyedService {
 public:
  // Mapping from credit card guid id to offer data.
  using CardLinkedOffersMap = std::map<std::string, const AutofillOfferData*>;

  explicit AutofillOfferManager(PaymentsDataManager* payments_data_manager);
  ~AutofillOfferManager() override;
  AutofillOfferManager(const AutofillOfferManager&) = delete;
  AutofillOfferManager& operator=(const AutofillOfferManager&) = delete;

  // Invoked when the navigation happens.
  void OnDidNavigateFrame(AutofillClient& client);

  // Updates the offer notification UI for the page that `client` currently has
  // committed.
  void UpdateOfferNotificationVisibility(AutofillClient& client);

  // Returns true only if the domain of `last_committed_primary_main_frame_url`
  // has an offer.
  bool IsUrlEligible(const GURL& last_committed_primary_main_frame_url);

  // Returns the offer that contains the domain of
  // `last_committed_primary_main_frame_url`.
  const AutofillOfferData* GetOfferForUrl(
      const GURL& last_committed_primary_main_frame_url) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      AutofillOfferManagerTest,
      CreateCardLinkedOffersMap_ReturnsOnlyCardLinkedOffers);
  FRIEND_TEST_ALL_PREFIXES(AutofillOfferManagerTest, IsUrlEligible);
  friend class OfferNotificationBubbleViewsInteractiveUiTest;
  friend class OfferNotificationControllerAndroidBrowserTest;

  const raw_ref<PaymentsDataManager> payments_data_manager_;

  // The handler for offer notification UI. It is a sub-level component of
  // AutofillOfferManager to decide whether to show the offer notification.
  OfferNotificationHandler notification_handler_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_
