// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_

#include <stdint.h>
#include <map>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace autofill {

// Manages all Autofill related offers. One per frame; owned by the
// AutofillManager.
class AutofillOfferManager : public KeyedService,
                             public PersonalDataManagerObserver {
 public:
  // Mapping from suggestion backend ID to offer data.
  using OffersMap = std::map<std::string, AutofillOfferData*>;

  explicit AutofillOfferManager(PersonalDataManager* personal_data);
  ~AutofillOfferManager() override;
  AutofillOfferManager(const AutofillOfferManager&) = delete;
  AutofillOfferManager& operator=(const AutofillOfferManager&) = delete;

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  // Modifies any suggestion in |suggestions| if it has related offer data.
  void UpdateSuggestionsWithOffers(const GURL& last_committed_url,
                                   std::vector<Suggestion>& suggestions);

 private:
  // Queries |personal_data_| to reset the elements of
  // |eligible_merchant_domains_|
  void UpdateEligibleMerchantDomains();

  // Creates a mapping from Suggestion Backend ID's to eligible Credit Card
  // Offers.
  OffersMap CreateOffersMap(const GURL& last_committed_url_origin) const;

  PersonalDataManager* personal_data_;
  std::set<GURL> eligible_merchant_domains_ = {};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_OFFER_MANAGER_H_
