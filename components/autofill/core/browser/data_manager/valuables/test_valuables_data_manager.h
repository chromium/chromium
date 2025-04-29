// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_TEST_VALUABLES_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_TEST_VALUABLES_DATA_MANAGER_H_

#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/ui/test_autofill_image_fetcher.h"

namespace autofill {

// A simplistic ValuablesDataManager used for testing.
class TestValuablesDataManager : public ValuablesDataManager {
 public:
  TestValuablesDataManager();
  TestValuablesDataManager(const TestValuablesDataManager&) = delete;
  TestValuablesDataManager& operator=(const TestValuablesDataManager&) = delete;
  ~TestValuablesDataManager() override;

  // Adds a `url` to `image` mapping to the local `credit_card_art_images_`
  // cache.
  void CacheImage(const GURL& url, const gfx::Image& image);

 private:
  std::unique_ptr<TestAutofillImageFetcher> owned_image_fetcher_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_VALUABLES_TEST_VALUABLES_DATA_MANAGER_H_
