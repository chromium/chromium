// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PASSES_PASSES_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PASSES_PASSES_DATA_MANAGER_H_

#include <string_view>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/autofill/core/browser/data_model/passes/loyalty_card.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// Loads non-payments data types coming from the Google Wallet like loyalty
// cards.
//
// These operations are asynchronous; this is similar to
// `AutocompleteHistoryManager` and unlike `AddressDataManager`.
//
// A shared instance of this service is created for regular and off-the-record
// profiles. Future modifications to this service must make sure that no data is
// persisted for the off-the-record profile.
//
// TODO: crbug.com/393123618 - make the loading API synchronous.
class PassesDataManager : public KeyedService {
 public:
  using LoadCallback = base::OnceCallback<void(std::vector<LoyaltyCard>)>;

  explicit PassesDataManager(
      scoped_refptr<AutofillWebDataService> webdata_service);
  PassesDataManager(const PassesDataManager&) = delete;
  PassesDataManager& operator=(const PassesDataManager&) = delete;
  ~PassesDataManager() override;

  // Retrieves the loyalty cards from the database and calls `cb` asynchronously
  // with the result.
  //
  // It is guaranteed that `cb` is called eventually; if the query is
  // unsuccessful, `cb` is called with an empty vector.
  void GetLoyaltyCards(LoadCallback cb);

 private:
  const scoped_refptr<AutofillWebDataService> webdata_service_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PASSES_PASSES_DATA_MANAGER_H_
