// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_AUTOFILL_CROWDSOURCING_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_AUTOFILL_CROWDSOURCING_MANAGER_TEST_API_H_

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.h"

namespace autofill {

// Friend class to AutofillCrowdsourcingManager to allow tests to interact with
// and query the internal components of the class.
class AutofillCrowdsourcingManagerTestApi {
 public:
  static std::unique_ptr<AutofillCrowdsourcingManager> CreateManagerForApiKey(
      AutofillClient* client,
      const std::string& api_key) {
    return base::WrapUnique(
        new AutofillCrowdsourcingManager(client, api_key,
                                         /*log_manager=*/nullptr));
  }

  explicit AutofillCrowdsourcingManagerTestApi(
      AutofillCrowdsourcingManager* manager)
      : manager_(*manager) {}

  // Returns the current backoff time. A non-zero backoff time does not mean
  // that there will be a request once that time has passed, as there may be
  // other reasons not to send the request.
  base::TimeDelta GetCurrentBackoffTime() && {
    return manager_->loader_backoff_.GetTimeUntilRelease();
  }

  void set_max_form_cache_size(size_t max_form_cache_size) {
    manager_->max_form_cache_size_ = max_form_cache_size;
  }

 private:
  const raw_ref<AutofillCrowdsourcingManager> manager_;
};

inline AutofillCrowdsourcingManagerTestApi test_api(
    AutofillCrowdsourcingManager& manager) {
  return AutofillCrowdsourcingManagerTestApi(&manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CROWDSOURCING_AUTOFILL_CROWDSOURCING_MANAGER_TEST_API_H_
