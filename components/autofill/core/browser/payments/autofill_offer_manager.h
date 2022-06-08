// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "components/autofill/core/browser/payments/offer_notification_handler.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace autofill {

class AutofillClient;
class AutofillOfferData;
class OfferNotificationHandler;
class PersonalDataManager;
struct Suggestion;

// A delegate class to expose relevant CouponService functionalities.
class CouponServiceDelegate {
 public:
  // Get FreeListing coupons for the given URL. Will return an empty
  // list if there is no coupon data associated with this URL.
  virtual std::vector<AutofillOfferData*> GetFreeListingCouponsForUrl(
      const GURL& url) = 0;

  // Check if CouponService has eligible coupons for |last_committed_url|.
  virtual bool IsUrlEligible(const GURL& last_committed_url) = 0;

 protected:
  virtual ~CouponServiceDelegate() = default;
};

// Manages all Autofill related offers. One per browser context. Owned and
// created by the AutofillOfferManagerFactory.
class AutofillOfferManager : public KeyedService,
                             public PersonalDataManagerObserver {
 public:
  // Mapping from suggestion backend ID to offer data.
  using OffersMap = std::map<std::string, AutofillOfferData*>;

  AutofillOfferManager(PersonalDataManager* personal_data,
                       CouponServiceDelegate* coupon_service_delegate);
  ~AutofillOfferManager() override;
  AutofillOfferManager(const AutofillOfferManager&) = delete;
  AutofillOfferManager& operator=(const AutofillOfferManager&) = delete;

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  // Invoked when the navigation happens.
  void OnDidNavigateFrame(AutofillClient* client);

  // Modifies any suggestion in |suggestions| if it has related offer data.
  void UpdateSuggestionsWithOffers(const GURL& last_committed_url,
                                   std::vector<Suggestion>& suggestions);

  // Returns true only if the domain of |last_committed_url| has an offer.
  bool IsUrlEligible(const GURL& last_committed_url);

  // Returns the offer that contains the domain of |last_committed_url|.
  AutofillOfferData* GetOfferForUrl(const GURL& last_committed_url);

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

  // Creates a mapping from Suggestion Backend ID's to eligible card-linked
  // offers.
  OffersMap CreateCardLinkedOffersMap(
      const GURL& last_committed_url_origin) const;

  raw_ptr<PersonalDataManager> personal_data_;
  raw_ptr<CouponServiceDelegate> coupon_service_delegate_;

  // This set includes all the eligible domains where offers are applicable.
  // This is used as a local cache and will be updated whenever the data in the
  // database changes.
  std::set<GURL> eligible_merchant_domains_ = {};

  // The handler for offer notification UI. It is a sub-level component of
  // AutofillOfferManager to decide whether to show the offer notification.
  OfferNotificationHandler notification_handler_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_
