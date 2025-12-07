// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_PASSKEY_AFFILIATION_SOURCE_ADAPTER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_PASSKEY_AFFILIATION_SOURCE_ADAPTER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/affiliations/core/browser/affiliation_source.h"
#include "components/webauthn/core/browser/passkey_model.h"

namespace password_manager {

// This class represents a source for passkey-related data requiring
// affiliation updates. It utilizes PasskeyModel's information and monitors
// changes to notify observers.
class PasskeyAffiliationSourceAdapter
    : public affiliations::AffiliationSource,
      public webauthn::PasskeyModel::Observer {
 public:
  explicit PasskeyAffiliationSourceAdapter(
      webauthn::PasskeyModel* passkey_model);
  ~PasskeyAffiliationSourceAdapter() override;

  // AffiliationSource:
  void GetFacets(AffiliationSource::ResultCallback response_callback) override;
  void StartObserving(AffiliationSource::Observer* observer) override;

 private:
  // webauthn::PasskeyModel::Observer:
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;
  void OnPasskeyModelIsReady(bool is_ready) override;

  raw_ptr<webauthn::PasskeyModel> passkey_model_ = nullptr;
  raw_ptr<AffiliationSource::Observer> observer_ = nullptr;

  // Observer to `passkey_model_`.
  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      passkey_model_observation_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_PASSKEY_AFFILIATION_SOURCE_ADAPTER_H_
