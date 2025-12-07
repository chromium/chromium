// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_AUTOFILL_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_AUTOFILL_MANAGER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/foundations/autofill_driver_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"

namespace autofill {

// Exposes some testing operations for AutofillManager.
class AutofillManagerTestApi {
 public:
  explicit AutofillManagerTestApi(AutofillManager* manager)
      : manager_(*manager) {}

  const base::ObserverList<AutofillManager::Observer>& observers() {
    return manager_->observers_;
  }

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

  FormStructure* AddSeenFormStructure(
      std::unique_ptr<FormStructure> form_structure) {
    const FormGlobalId id = form_structure->global_id();
    manager_->form_structures_[id] = std::move(form_structure);
    return manager_->form_structures_[id].get();
  }

  void ClearFormStructures() { manager_->form_structures_.clear(); }

 private:
  raw_ref<AutofillManager> manager_;
};

inline AutofillManagerTestApi test_api(AutofillManager& manager) {
  return AutofillManagerTestApi(&manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_AUTOFILL_MANAGER_TEST_API_H_
