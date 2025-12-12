// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/valuables/test_valuables_data_manager.h"

namespace autofill {

TestValuablesDataManager::TestValuablesDataManager()
    : ValuablesDataManager(/*webdata_service=*/nullptr,
                           /*pref_service=*/nullptr,
                           /*image_fetcher=*/nullptr) {
  owned_image_fetcher_ = std::make_unique<TestAutofillImageFetcher>();
  image_fetcher_ = owned_image_fetcher_.get();
}

TestValuablesDataManager::~TestValuablesDataManager() {
  // Clear `image_fetcher_` raw pointer because the `owned_image_fetcher_` goes
  // first out of scope.
  image_fetcher_ = nullptr;
}

void TestValuablesDataManager::CacheImage(const GURL& url,
                                          const gfx::Image& image) {
  owned_image_fetcher_->CacheImage(url, image);
}

bool TestValuablesDataManager::IsAutofillPaymentMethodsEnabled() const {
  return true;
}

}  // namespace autofill
