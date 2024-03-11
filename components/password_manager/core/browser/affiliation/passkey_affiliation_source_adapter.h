// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_PASSKEY_AFFILIATION_SOURCE_ADAPTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_PASSKEY_AFFILIATION_SOURCE_ADAPTER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/affiliations/core/browser/affiliation_source.h"

namespace webauthn {
class PasskeyModel;
}

namespace password_manager {

// This class represents a source for passkey-related data requiring
// affiliation updates. It utilizes PasskeyModel's information and monitors
// changes to notify observers.
class PasskeyAffiliationSourceAdapter : public affiliations::AffiliationSource {
 public:
  PasskeyAffiliationSourceAdapter(webauthn::PasskeyModel* passkey_model,
                                  AffiliationSource::Observer* observer);
  ~PasskeyAffiliationSourceAdapter() override;

  // AffiliationSource:
  void GetFacets(AffiliationSource::ResultCallback response_callback) override;
  void StartObserving() override;

 private:
  const raw_ptr<webauthn::PasskeyModel> passkey_model_;
  const raw_ref<AffiliationSource::Observer> observer_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_PASSKEY_AFFILIATION_SOURCE_ADAPTER_H_
