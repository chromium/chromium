// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DOWNLOAD_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DOWNLOAD_MANAGER_H_

#include <vector>

#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/common/form_data.h"

namespace autofill {

class FormStructure;

class TestAutofillDownloadManager : public AutofillDownloadManager {
 public:
  TestAutofillDownloadManager(AutofillDriver* driver,
                              AutofillDownloadManager::Observer* observer);

  TestAutofillDownloadManager(const TestAutofillDownloadManager&) = delete;
  TestAutofillDownloadManager& operator=(const TestAutofillDownloadManager&) =
      delete;

  ~TestAutofillDownloadManager() override;

  // AutofillDownloadManager overrides.
  bool StartQueryRequest(const std::vector<FormStructure*>& forms) override;

  // Unique to TestAutofillDownloadManager:

  // Verify that the last queried forms equal |expected_forms|.
  void VerifyLastQueriedForms(const std::vector<FormData>& expected_forms);

 private:
  std::vector<FormStructure*> last_queried_forms_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_AUTOFILL_DOWNLOAD_MANAGER_H_
