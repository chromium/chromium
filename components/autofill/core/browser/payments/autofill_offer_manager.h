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
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/autofill/core/browser/payments/offer_notification_handler.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/commerce/core/commerce_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace autofill {

class AutofillClient;
class AutofillOfferData;
class OfferNotificationHandler;
class PersonalDataManager;

// A delegate class that exposes ShoppingService functionalities.
// This component can't depend on commerce:shopping_service component target
// because of crbug.com/1155712. We should remove this and use the shopping
// service directly once this is fixed.
class ShoppingServiceDelegate {
 public:
  virtual ~ShoppingServiceDelegate() = default;
  virtual bool IsDiscountEligibleToShowOnNavigation() = 0;
  virtual void GetDiscountInfoForUrls(
      const std::vector<GURL>& urls,
      commerce::DiscountInfoCallback callback) = 0;
};

// A delegate class to expose relevant CouponService functionalities.
class CouponServiceDelegate {
 public:
  // Get FreeListing coupons for the given URL. Will return an empty
  // list if there is no coupon data associated with this URL.
  virtual std::vector<AutofillOfferData*> GetFreeListingCouponsForUrl(
      const GURL& url) = 0;

  // Check if CouponService has eligible coupons for
  // |last_committed_primary_main_frame_url|.
  virtual bool IsUrlEligible(
      const GURL& last_committed_primary_main_frame_url) = 0;

 protected:
  virtual ~CouponServiceDelegate() = default;
};

// Manages all Autofill related offers. One per browser context. Owned and
// created by the AutofillOfferManagerFactory.
class AutofillOfferManager : public KeyedService,
                             public PersonalDataManagerObserver {
 public:
  // Mapping from credit card guid id to offer data.
  using CardLinkedOffersMap = std::map<std::string, AutofillOfferData*>;
  // Callbacks for getting offers asynchronously, primarily used by calls to the
  // ShoppingService.
  using AsyncOfferCallback =
      base::OnceCallback<void(const GURL&, const AutofillOfferData&)>;

  AutofillOfferManager(
      PersonalDataManager* personal_data,
      CouponServiceDelegate* coupon_service_delegate,
      std::unique_ptr<ShoppingServiceDelegate> shopping_service_delegate);
  ~AutofillOfferManager() override;
  AutofillOfferManager(const AutofillOfferManager&) = delete;
  AutofillOfferManager& operator=(const AutofillOfferManager&) = delete;

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  // Invoked when the navigation happens.
  void OnDidNavigateFrame(AutofillClient* client);

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

  // Get offer for `url` asynchronously from ShoppingService. If there are any
  // available offers, `callback` will be called.
  void GetShoppingServiceOfferForUrl(const GURL& url,
                                     AsyncOfferCallback callback);

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

  void HandleShoppingServiceResponse(const GURL& url,
                                     AsyncOfferCallback callback,
                                     const commerce::DiscountsMap& discounts);

  raw_ptr<PersonalDataManager> personal_data_;
  raw_ptr<CouponServiceDelegate> coupon_service_delegate_;
  std::unique_ptr<ShoppingServiceDelegate> shopping_service_delegate_;

  // This set includes all the eligible domains where offers are applicable.
  // This is used as a local cache and will be updated whenever the data in the
  // database changes.
  std::set<GURL> eligible_merchant_domains_ = {};

  base::ScopedObservation<PersonalDataManager, PersonalDataManagerObserver>
      personal_data_manager_observation{this};

  // The handler for offer notification UI. It is a sub-level component of
  // AutofillOfferManager to decide whether to show the offer notification.
  OfferNotificationHandler notification_handler_{this};

  base::WeakPtrFactory<AutofillOfferManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_
