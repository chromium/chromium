// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/autofill_driver_test_api.h"
#include "components/autofill/core/browser/autofill_manager.h"

namespace autofill {

// Exposes some testing operations for AutofillManager.
class AutofillManagerTestApi {
 public:
  explicit AutofillManagerTestApi(AutofillManager* manager)
      : manager_(*manager) {}

  const base::ObserverList<AutofillManager::Observer>& observers() {
    return manager_->observers_;
  }

  void Reset() { manager_->Reset(); }

  void OnLoadedServerPredictions(
      std::string response,
      const std::vector<FormSignature>& queried_form_signatures) {
    OnLoadedServerPredictions(AutofillCrowdsourcingManager::QueryResponse{
        response, queried_form_signatures});
  }

  void OnLoadedServerPredictions(
      AutofillCrowdsourcingManager::QueryResponse response) {
    manager_->NotifyObservers(
        &AutofillManager::Observer::OnBeforeLoadedServerPredictions);
    manager_->OnLoadedServerPredictions(std::move(response));
  }

  void OnFormsParsed(const std::vector<FormData>& forms) {
    manager_->OnFormsParsed(forms);
  }

  std::map<FormGlobalId, std::unique_ptr<FormStructure>>*
  mutable_form_structures() {
    return manager_->mutable_form_structures();
  }

 private:
  raw_ref<AutofillManager> manager_;
};

inline AutofillManagerTestApi test_api(AutofillManager& manager) {
  return AutofillManagerTestApi(&manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_TEST_API_H_
